// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "event.h"
#include "utils/meta.h"
#include "utils/aws.h"

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/DynamoDBRequest.h>
#include <aws/dynamodb/model/PutItemRequest.h>

namespace sentio::tracking
{

class location_log::impl
{
public:
  impl(std::string tablename)
      : table_(std::move(tablename))
  {
  }

  void store_location_event(location_event ev) const
  {
    using namespace Aws::DynamoDB;
    namespace bpt = boost::posix_time;
    namespace aws_db = Aws::DynamoDB::Model;

    verify_argument(!ev.accountid.empty());
    verify_argument(!ev.event_type.empty());

    auto requestobj = std::make_shared<aws_db::PutItemRequest>();
    requestobj->WithTableName(table_)
        .AddItem("accountid", aws_db::AttributeValue(ev.accountid))
        .AddItem("timestamp",
                 aws_db::AttributeValue(bpt::to_iso_string(ev.timestamp)))
        .AddItem("location", sentio::aws::as_av(ev.location))
        .AddItem("event_type", aws_db::AttributeValue(ev.event_type))
        .AddItem("event_params", aws_db::AttributeValue(ev.event_params));

    ddbclient_.PutItemAsync(
        *requestobj, [ev = std::move(ev), requestobj](
                         auto*, auto const&, auto const& result, auto const&) {
          if (!result.IsSuccess()) {
            std::cerr << "failed to store location event: " << ev.event_type
                      << " for " << ev.accountid << ": " << result.GetError()
                      << std::endl;
          }
        });
  }

  void store_location_events(std::vector<location_event> evs) const
  {
    boost::ignore_unused_variable_warning(evs);
    throw std::runtime_error("not implemented");
  }

private:
  std::string table_;
  Aws::DynamoDB::DynamoDBClient ddbclient_;
};

location_log::~location_log() = default;
location_log::location_log(location_log&&) = default;

location_log::location_log(std::string tablename)
    : impl_(new impl(std::move(tablename)))
{
}

void location_log::store_location_event(location_event ev) const
{
  impl_->store_location_event(std::move(ev));
}

void location_log::store_location_events(std::vector<location_event> evs) const
{
  impl_->store_location_events(std::move(evs));
}

}