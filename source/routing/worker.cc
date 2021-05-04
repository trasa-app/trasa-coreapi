// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <mutex>
#include <thread>
#include <unordered_map>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/core/utils/json/JsonSerializer.h>

#include <boost/property_tree/json_parser.hpp>

#include "worker.h"
#include "scheduler.h"
#include "osrm_interop.h"
#include "utils/aws.h"
#include "utils/log.h"
#include "utils/future.h"
#include "import/map_source.h"

namespace sentio::routing
{

void persist_complete_trip(
  trip_response const& tr, 
  Aws::DynamoDB::DynamoDBClient const& ddbclient)
{ 
  namespace bpt = boost::posix_time;
  namespace aws_db = Aws::DynamoDB::Model;

  std::stringstream reqss, resss;
  auto responsejson = tr.to_json();

  bool pretty = false;
  boost::property_tree::write_json(reqss, 
    responsejson.get_child("request"), pretty);
  boost::property_tree::write_json(resss, 
    responsejson.get_child("response"), pretty);

  auto const putresult = ddbclient.PutItem(
    aws_db::PutItemRequest()
      .WithTableName(aws::resources().tables.trips)
      .AddItem("id", aws_db::AttributeValue(tr.meta().id().value()))
      .AddItem("timestamp", aws_db::AttributeValue(
        bpt::to_iso_string(bpt::second_clock::universal_time())))
      .AddItem("accountid", aws_db::AttributeValue(
        tr.meta().accountid()))
      .AddItem("status", aws_db::AttributeValue("ready"))
      .AddItem("region", aws_db::AttributeValue(
        tr.meta().region()))
      .AddItem("request", aws_db::AttributeValue(reqss.str()))
      .AddItem("response", aws_db::AttributeValue(resss.str()))
      .AddItem("geometry", aws_db::AttributeValue(
        tr.trip().geometry().serialized()))
      .AddItem("distance", aws_db::AttributeValue().SetN(
        tr.trip().total_cost().distance))
      .AddItem("duration", aws_db::AttributeValue().SetN(
        (int)tr.trip().total_cost().duration.count())));

  if (!putresult.IsSuccess()) {
    errlog << "persisting trip "
              << tr.meta().id().value() << " failed "
              << putresult.GetError();
    throw std::runtime_error(putresult.GetError().GetMessage());
  }

  infolog << "persisted trip " << tr.meta().id().value()
            << " in dynamodb table " << aws::resources().tables.trips
           ;
}

void persist_discarded_trip(
  trip_metadata&& meta, 
  std::string const& error,
  Aws::DynamoDB::DynamoDBClient const& ddbclient)
{
  namespace bpt = boost::posix_time;
  namespace aws_db = Aws::DynamoDB::Model;

  assert(meta.id().has_value());
  assert(!meta.region().empty());
  assert(!meta.accountid().empty());

  auto const putresult = ddbclient.PutItem(
    aws_db::PutItemRequest()
      .WithTableName(aws::resources().tables.trips)
      .AddItem("id", aws_db::AttributeValue(meta.id().value()))
      .AddItem("timestamp", aws_db::AttributeValue(
        bpt::to_iso_string(bpt::second_clock::universal_time())))
      .AddItem("accountid", aws_db::AttributeValue(meta.accountid()))
      .AddItem("status", aws_db::AttributeValue("failed"))
      .AddItem("region", aws_db::AttributeValue(meta.region()))
      .AddItem("error", aws_db::AttributeValue(error)));

  if (!putresult.IsSuccess()) {
    errlog << "failed to persist failed trip request status for "
              << meta.id().value() << ": " << putresult.GetError() 
             ;
  }
}

void start_routing_worker(config const& config,
  std::vector<import::region_paths> const& sources)
{
  scheduler scheduler;
  osrm_map instancesmap(config, sources);
  Aws::DynamoDB::DynamoDBClient ddbclient;
  
  // how many workers for each core
  size_t worker_count =
    std::thread::hardware_concurrency() * 
    config.worker_concurrency;

  infolog << "using trip requests queue: " 
            << aws::resources().queues.pending_routes
           ;
  infolog << "starting " << worker_count 
            << " routing worker therads"
           ;

  std::list<std::thread> workers;
  for (size_t i = 0; i < worker_count; ++i) {
    workers.emplace_back(std::thread([&] {
      while (true) {
        auto nextrequest = scheduler.poll_trip_request();
        if (!nextrequest.has_value()) {
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        }

        // store a copy of the trip metadat in case it fails
        // so it can be discarded later on, and an appropriate
        // statuses persisted in the database.
        auto tripmeta = nextrequest->meta();

        try {
          // from the instances map, pick the osrm instance
          // that has the road network for the current trip region
          // and calculate an optimal trip
          auto optimized = instancesmap
            .optimize_trip(
              std::move(nextrequest->trip()),
              nextrequest->meta().region());

          // wrap it in a response object that also has the trip metadata
          auto tripresponse = trip_response(
            std::move(optimized), 
            std::move(tripmeta));

          // then pesist the calculated trip to dynamodb, so that
          // future calls to trip.poll will return the result of
          // this computation, rather than a "pending" status.
          persist_complete_trip(tripresponse, ddbclient);

          // mark this trip as complete and remove it from the
          // scheduler queue, so it won't be retried anymore.
          scheduler.complete_trip(std::move(tripresponse));
        } catch (std::exception const& e) {
          errlog << "trip " << nextrequest->meta().id().value()
                    << "failed and will be discarded permanently: " 
                    << e.what();
          scheduler.discard_trip(tripmeta);
          persist_discarded_trip(std::move(tripmeta), e.what(), ddbclient);
        }
        
      }
    }));
  }

  // block calling thread until all
  // worker threads are terminated.
  utils::join_all(
    workers.begin(), 
    workers.end());
}

}  // namespace sentio::routing
