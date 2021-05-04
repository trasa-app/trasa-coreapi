// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <boost/lexical_cast.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <aws/sqs/model/SendMessageResult.h>
#include <aws/sqs/model/SendMessageRequest.h>
#include <aws/sqs/model/ReceiveMessageResult.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/GetQueueAttributesRequest.h>

#include "scheduler.h"
#include "utils/log.h"
#include "utils/aws.h"

namespace sentio::routing
{

json_t trip_promise::to_json() const
{
  using namespace boost::posix_time;
  json_t output;
  output.add("id", id);
  output.add("expected_at", to_iso_string(expected_at));
  output.add("scheduled_at", to_iso_string(scheduled_at));
  return output;
}

scheduler::scheduler() {}

size_t scheduler::pending_promises() const 
{  
  using namespace Aws::SQS::Model;
  GetQueueAttributesRequest attrreq;
  attrreq.SetQueueUrl(aws::resources().queues.pending_routes);
  attrreq.AddAttributeNames(QueueAttributeName::ApproximateNumberOfMessages);
  auto result = sqs_.GetQueueAttributes(attrreq);
  if (!result.IsSuccess()) {
    errlog << result.GetError();
    throw std::runtime_error(result.GetError().GetMessage());
  } else {
    auto const& attribs = result.GetResult().GetAttributes();
    auto const& attribval = attribs.at(
      QueueAttributeName::ApproximateNumberOfMessages);
    return boost::lexical_cast<uint64_t>(attribval);
  }
}

trip_promise scheduler::schedule_trip(trip_request t) const 
{
  using namespace boost::posix_time;

  Aws::SQS::Model::SendMessageRequest queuemsg;
  queuemsg.SetQueueUrl(aws::resources().queues.pending_routes);
  
  try {
    std::stringstream ss;
    boost::property_tree::write_json(ss, t.to_json());
    queuemsg.SetMessageBody(ss.str());
  } catch (std::exception const& e) {
    errlog << e.what();
    throw;
  }
  auto result = sqs_.SendMessage(queuemsg);

  if (result.IsSuccess()) {
    return trip_promise {
      .id = result.GetResult().GetMessageId(),
      .expected_at = second_clock::universal_time() + seconds(3),
      .scheduled_at = second_clock::universal_time()
    };
  } else {
    errlog << result.GetError();
    throw std::runtime_error(result.GetError().GetMessage());
  }
}

void delete_message(std::string receipthandle, Aws::SQS::SQSClient const& sqs)
{
  Aws::SQS::Model::DeleteMessageRequest delmsg;
  delmsg.SetQueueUrl(aws::resources().queues.pending_routes);
  delmsg.SetReceiptHandle(std::move(receipthandle));
  auto result = sqs.DeleteMessage(delmsg);

  if (!result.IsSuccess()) {
    errlog << "failed to delete message with receipt id "
              << delmsg.GetReceiptHandle() << ": "
              << result.GetError();
  }
}

std::optional<trip_request> scheduler::poll_trip_request() const
{
  Aws::SQS::Model::ReceiveMessageRequest getmsg;
  getmsg.SetQueueUrl(aws::resources().queues.pending_routes);
  getmsg.SetMaxNumberOfMessages(1);

  auto result = sqs_.ReceiveMessage(getmsg);
  if (!result.IsSuccess()) {
    errlog << "error polling pending routes queue: "
              << result.GetError() << ". queue url: "
              << aws::resources().queues.pending_routes
             ;
    return {};
  }

  if (result.GetResult().GetMessages().size() == 0) { 
    return { }; 
  }
  auto const& msgs = result.GetResult().GetMessages();
  
  try {
    json_t requestjson;
    std::stringstream ss(msgs[0].GetBody());
    boost::property_tree::read_json(ss, requestjson);
    requestjson.add("meta.id", msgs[0].GetMessageId());
    requestjson.add("meta.receipthandle", msgs[0].GetReceiptHandle());
    trip_request trip(requestjson);
    infolog << "processing trip request with id " 
              << trip.meta().id().value() << " in region "
              << trip.meta().region() << " for account " 
              << trip.meta().accountid();
    return trip;
  } catch (std::exception const& e) {
    errlog << "failed receiving trip request with id "
              << msgs[0].GetMessageId() << ": " 
              << e.what();
    errlog << "deleting message id " 
              << msgs[0].GetMessageId() 
              << ", receipt handle: " 
              << msgs[0].GetReceiptHandle()
             ;
    delete_message(msgs[0].GetReceiptHandle(), sqs_);
    return {};
  }
}

void scheduler::complete_trip(trip_response&& response) const
{
  assert(response.meta().id().has_value());
  assert(response.meta().receipthandle().has_value());

  infolog << "removed trip request with id " 
            << response.meta().id().value()
            << " from scheduler queue.";

  delete_message(response.meta().receipthandle().value(), sqs_);
}

void scheduler::discard_trip(trip_metadata const& meta) const
{
  assert(meta.receipthandle().has_value());
  delete_message(meta.receipthandle().value(), sqs_);
}

}