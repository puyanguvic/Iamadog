/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef DSR_VIRTUAL_QUEUE_DISC_H
#define DSR_VIRTUAL_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3 {

class DSRVirtualQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief DSRVirtualQueueDisc constructor
   *
   * Creates a queue with a depth of 1000 packets per band by default
   */
  DSRVirtualQueueDisc ();

  virtual ~DSRVirtualQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded

private:
  int bufferSize [3] = {100, 200, 300};
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  bool CheckConfig (void);
  virtual void InitializeParams (void);
};

} // namespace ns3

#endif /* DSR_VIRTUAL_QUEUE_DISC_H */
