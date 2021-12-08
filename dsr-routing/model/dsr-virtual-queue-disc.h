/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef DSR_VIRTUAL_QUEUE_DISC_H
#define DSR_VIRTUAL_QUEUE_DISC_H

#include "ns3/queue-disc.h"

namespace ns3 {

class DsrVirtualQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PfifoFastQueueDisc constructor
   *
   * Creates a queue with a depth of 1000 packets per band by default
   */
  DsrVirtualQueueDisc ();

  virtual ~DsrVirtualQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded
  static constexpr const char* TIMEOUT_DROP = "time out !!!!!!!!";

private:
  // packet size = 1kB
  // packet size for test = 52B
  uint32_t LinesSize[3] = {12,36,100};
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);
};

}

#endif /* DSR_VIRTUAL_QUEUE_DISC_H */
