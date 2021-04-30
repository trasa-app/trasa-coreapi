// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string> 

#include "trip.h"

#include "utils/json.h"
#include "utils/datetime.h"

#include <aws/sqs/SQSClient.h>

namespace sentio::routing
{
/**
 * A Handle to a scheduled trip request.
 * 
 * This object is generated whenever a long trip is 
 * added to the pending queue. Using this object clients
 * can query for the status of the trip and retreive it
 * once its complete
 */
struct trip_promise {
  std::string id;
  date_time_t expected_at;
  date_time_t scheduled_at;
  json_t to_json() const;
};

/**
 * Used to schedule long trips for async background processing.
 * 
 * This is used when trips have a large amount of waypoints above
 * the async_threashold (defined in the config.json file).
 * 
 * This allows for more scalable processing as it won't block the 
 * RPC server for long periods. Processing longer trips can take up
 * to few seconds and its essential not to bring the server down with
 * high number of requests, its better to make users wait a bit 
 * longer for their trips with heavier load rather than kill the server.
 */
class scheduler
{
public:
  scheduler();

public:
  /**
   * Returns the number of all trips from all instances of 
   * the server that are awaiting to be optimized. This is 
   * used as a datapoint to calculate the ETA for the trip
   * promise.
   * 
   * Currently this implementation checks the number of pending
   * messages in the AWS SQS queue.
   */
  size_t pending_promises() const;

  /**
   * Enqueues a trip for being optimized in the future.
   * 
   * This is used for longer trips that have large number
   * of waypoints, where calculating them synchronously is 
   * not practical.
   * 
   * This implementation will serialize a JSON representation
   * of the trip in SQS and return an object that can be used to
   * query for the trip status and its ETA.
   */
  trip_promise schedule_trip(trip_request) const;

  /**
   * Gets next available trip routing request off the scheduler 
   * queue, and makes it invisible for other workers for about 10 seconds.
   * 
   * Once one of the workers calculates a response to the trip, it should
   * call @c complete_trip, which will permanently remove the request
   * from the queue and stop any further attempts at scheduling it for 
   * route calculation.
   */
  std::optional<trip_request> poll_trip_request() const;

  /**
   * Called by the trip worker once a trip is calculated and stored in
   * a permanent trip store for future retrieval by client requests to
   * "trip.poll" rpc method. 
   * 
   * This method will remove the trip request from the queue and it wont
   * be retried anymore by routing workers.
   */
  void complete_trip(trip_response&&) const;

  /**
   * Called when a trip request fails unrecoverable.
   * This will remove it from the queue.
   */
  void discard_trip(trip_metadata const&) const;

private:
  Aws::SQS::SQSClient sqs_;
};

}