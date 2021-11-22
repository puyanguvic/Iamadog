/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "ns3/tag.h"
#include "dsr-virtual-queue-disc.h"
#include "priority-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DSRVirtualQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DSRVirtualQueueDisc);

TypeId DSRVirtualQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DSRVirtualQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("dsr-routing")
    .AddConstructor<DSRVirtualQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

DSRVirtualQueueDisc::DSRVirtualQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

DSRVirtualQueueDisc::~DSRVirtualQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
DSRVirtualQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  PriorityTag priority;
  bool retval;
  uint32_t band;
  if (item->GetPacket ()->PeekPacketTag (priority))
    {
      band = priority.GetPriority();
      if (GetInternalQueue (band)->GetCurrentSize () >= GetInternalQueue (band)->GetMaxSize ())
        {
          NS_LOG_LOGIC ("Queue disc limit exceeded -- dropping packet");
          DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
          return false;
        }
      retval = GetInternalQueue (band)->Enqueue (item);
      NS_LOG_LOGIC ("Number packets band " << band << ": " << GetInternalQueue (band)->GetNPackets ());
    }

  retval = GetInternalQueue (3)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  if (!retval)
    {
      NS_LOG_WARN ("Packet enqueue failed. Check the size of the internal queues");
    }
  return retval;
}

Ptr<QueueDiscItem>
DSRVirtualQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;
  /**
   * \brief need more discuss how to implement the WRR queue
  */
  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Dequeue ()) != 0)
        {
          NS_LOG_LOGIC ("Popped from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }
  
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
DSRVirtualQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
DSRVirtualQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNInternalQueues () != 3)
    {
      NS_LOG_ERROR ("DSRVirtualQueueDisc needs 3 internal queues");
      return false;
    }

  return true;
}

void
DSRVirtualQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  ObjectFactory fastLane;
  fastLane.SetTypeId ("ns3::DropTailQueue<Packet>");
  fastLane.Set ("MaxSize", QueueSizeValue (QueueSize ("100p")));
  ObjectFactory slowLane;
  slowLane.SetTypeId ("ns3::DropTailQueue<Packet>");
  slowLane.Set ("MaxSize", QueueSizeValue (QueueSize ("200p")));

  ObjectFactory normalLane;
  normalLane.SetTypeId ("ns3::DropTailQueue<Packet>");
  normalLane.Set ("MaxSize", QueueSizeValue (QueueSize ("300p")));
  
  AddInternalQueue (fastLane.Create<InternalQueue> ());
  AddInternalQueue (slowLane.Create<InternalQueue> ());
  AddInternalQueue (normalLane.Create<InternalQueue> ());
}

} // namespace ns3
