// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "store.h"
#include "utils/log.h"
#include "utils/aws.h"
#include "utils/meta.h"

#include <string>
#include <vector>
#include <optional>
#include <iterator>
#include <functional>
#include <unordered_set>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

#include <boost/container_hash/hash.hpp>
#include <boost/algorithm/string.hpp>

namespace sentio::account
{
/**
 * Extracts a list of devices from an account and transforms them
 * to an instance of AttributeValue that can be used when inserting
 * or updating dynamodb.
 *
 * Accounts must always have at least one device.
 */
template <typename It>
Aws::DynamoDB::Model::AttributeValue devices_list(It beg, It end)
{
  using namespace sentio::aws;
  using namespace boost::posix_time;
  using namespace Aws::DynamoDB::Model;

  // can't have an account without at least one device
  verify_argument(std::distance(beg, end) != 0);

  AttributeValue outdevices;
  for (auto deviceit = beg; deviceit != end; ++deviceit) {
    outdevices.AddLItem(av(AttributeValue()
      .AddMEntry("os_version", av(deviceit->os_version))
      .AddMEntry("hardware_model", av(deviceit->hardware_model))
      .AddMEntry("platform", av(to_string(deviceit->platform)))
      .AddMEntry("first_use", av(to_iso_string(deviceit->first_use)))));
  }
  return outdevices;
}

template <typename RangeT>
Aws::DynamoDB::Model::AttributeValue devices_list(RangeT const& r)
{ return devices_list(std::begin(r), std::end(r)); }

/**
 * Extracts a list of restrictions placed on an account and transforms them
 * to an instance of AttributeValue that can be used when inserting or updating
 * dynamodb.
 *
 * Accounts may have zero or more restrictions placed on them.
 */
Aws::DynamoDB::Model::AttributeValue restrictions_list(account const& account)
{
  using namespace sentio::aws;
  using namespace boost::posix_time;
  using namespace Aws::DynamoDB::Model;

  AttributeValue restrictions;
  if (account.restrictions.size() != 0) {
    for (size_t i = 0; i < account.restrictions.size(); ++i) {
      AttributeValue restriction;
      date_time_t dt(account.restrictions[i].created_at);
      restriction.AddMEntry("enabled", av(account.restrictions[i].enabled));
      restriction.AddMEntry("reason", av(account.restrictions[i].reason));
      restriction.AddMEntry("type", av(account.restrictions[i].type));
      restriction.AddMEntry("params", av(account.restrictions[i].params));
      restriction.AddMEntry("created_at", av(to_iso_string(dt)));
      restrictions.AddLItem(av(std::move(restriction)));
    }
  } else {
    restrictions.SetL({});
  }

  return restrictions;
}


/**
 * Implementation details of the accounts repository.
 * This implementation uses dynamodb as its backing store.
 */
class repository::impl
{
public:
  /**
   * Creates an instance of the accounts repository and
   * configures the underlying dynamodb client.
   *
   * The table name comes most likely from env variables,
   * and is set in terraform scripts during deployment, then
   * passed to this executable through environment variables in
   * the ECS task definition.
   */
  impl(std::string table)
    : table_(std::move(table))
    , ddbclient_(Aws::Client::ClientConfiguration())
  { verify_argument(!table_.empty()); }

public:
  std::optional<account> retrieve(std::string uid) const
  {
    namespace bpt = boost::posix_time;

    verify_argument(!uid.empty());

    Aws::DynamoDB::Model::GetItemRequest request;
    Aws::DynamoDB::Model::AttributeValue primarykey(std::move(uid));

    request.SetTableName(table_);
    request.AddKey("uid", std::move(primarykey));
    auto const& result = ddbclient_.GetItem(request);

    if (!result.IsSuccess()) {
      errlog << result.GetError();
      throw std::runtime_error(result.GetError().GetMessage());
    }

    if (auto const& item = result.GetResult().GetItem(); item.size() != 0) {
      account output;

      // unique id
      output.uid = item.at("uid").GetS();

      // devices
      for (auto const& accdevice : item.at("devices").GetL()) {
        auto const& devmap = accdevice->GetM();
        output.devices.emplace_back(device{
            .platform = from_string(devmap.at("platform")->GetS()),
            .os_version = devmap.at("os_version")->GetS(),
            .hardware_model = devmap.at("hardware_model")->GetS(),
            .first_use = bpt::from_iso_string(devmap.at("first_use")->GetS())});
      }

      // restrictions
      for (auto const& accrestrict : item.at("restrictions").GetL()) {
        auto const& restrictmap = accrestrict->GetM();
        output.restrictions.emplace_back(restriction{
            .enabled = restrictmap.at("enabled")->GetBool(),
            .reason = restrictmap.at("reason")->GetS(),
            .type = restrictmap.at("type")->GetS(),
            .params = restrictmap.at("params")->GetS(),
            .created_at =
                bpt::from_iso_string(restrictmap.at("created_at")->GetS())});
      }

      // created at
      output.created_at = bpt::from_iso_string(item.at("created_at").GetS());
      return output;  // account object fully populated
    }

    return {};  // empty optional, account does not exist
  }

  account update(account newaccount, account const& oldaccount) const
  {
    using namespace Aws::DynamoDB::Model;

    verify_argument(newaccount.uid == oldaccount.uid);
    verify_argument(newaccount.created_at == oldaccount.created_at);

    // devices
    std::unordered_set<device> known_devices(oldaccount.devices.begin(),
                                             oldaccount.devices.end());
    std::unordered_set<device> new_devices(
        std::make_move_iterator(newaccount.devices.begin()),
        std::make_move_iterator(newaccount.devices.end()));
    new_devices.merge(std::move(known_devices));

    UpdateItemRequest request;
    AttributeValue uidvalue;
    request.SetTableName(table_);
    request.AddKey("uid", AttributeValue(newaccount.uid));
    request.SetUpdateExpression("SET devices = :merged_devices");
    Aws::Map<std::string, AttributeValue> update_attribs{
        {":merged_devices", devices_list(new_devices)}};
    request.SetExpressionAttributeValues(std::move(update_attribs));

    auto const& update_result = ddbclient_.UpdateItem(request);
    if (!update_result.IsSuccess()) {
      errlog << update_result.GetError();
      throw std::runtime_error(update_result.GetError().GetMessage());
    }

    return *retrieve(newaccount.uid);
  }

  account insert(account account) const
  {
    namespace bpt = boost::posix_time;
    namespace aws_db = Aws::DynamoDB::Model;

    verify_argument(!account.uid.empty());
    verify_argument(account.devices.size() == 1);
    verify_argument(!account.devices[0].os_version.empty());
    verify_argument(!account.devices[0].hardware_model.empty());

    // set the creation date to now()
    account.created_at = bpt::second_clock::universal_time();
    account.devices[0].first_use = bpt::second_clock::universal_time();

    auto const result = ddbclient_.PutItem(
        aws_db::PutItemRequest()
            .WithTableName(table_)
            .AddItem("uid", aws_db::AttributeValue(account.uid))
            .AddItem("devices", devices_list(account.devices))
            .AddItem("restrictions", restrictions_list(account))
            .AddItem("created_at", aws_db::AttributeValue(bpt::to_iso_string(
                                       account.created_at))));
    if (!result.IsSuccess()) {
      errlog << result.GetError();
      throw std::runtime_error(result.GetError().GetMessage());
    }
    return account;
  }

  account upsert(account account) const
  {
    verify_argument(!account.uid.empty());
    if (auto existingacc = retrieve(account.uid); existingacc.has_value()) {
      return update(std::move(account), *existingacc);
    } else {  // insert, account not stored yet
      return insert(std::move(account));
    }
  }

private:
  std::string table_;
  Aws::DynamoDB::DynamoDBClient ddbclient_;
};

repository::repository(std::string table)
  : impl_(new impl(std::move(table)))
{
}

std::optional<account> repository::retrieve(std::string uid) const
{ return impl_->retrieve(std::move(uid)); }

account repository::upsert(account account) const
{ return impl_->upsert(std::move(account)); }

repository::~repository() = default;
repository::repository(repository&&) = default;

}
