/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright(c) 2019 Intel Corporation
 */
#include "counter.h"
/* for GetDesc() */
#include "utils/format.h"
/*----------------------------------------------------------------------------------*/
const Commands Counter::cmds = {
    {"add", "CounterAddArg", MODULE_CMD_FUNC(&Counter::AddCounter),
     Command::THREAD_SAFE},
    {"remove", "CounterRemoveArg", MODULE_CMD_FUNC(&Counter::RemoveCounter),
     Command::THREAD_SAFE}};
/*----------------------------------------------------------------------------------*/
CommandResponse Counter::AddCounter(const bess::pb::CounterAddArg &arg) {
  uint32_t ctr_id = arg.ctr_id();

  if (counters.find(ctr_id) == counters.end()) {
    SessionStats s = {.pkt_count = 0, .byte_count = 0};
    counters.insert(std::pair<uint32_t, SessionStats>(ctr_id, s));
  } else
    return CommandFailure(EINVAL, "Unable to add ctr");
  return CommandSuccess();
}
/*----------------------------------------------------------------------------------*/
CommandResponse Counter::RemoveCounter(const bess::pb::CounterRemoveArg &arg) {
  uint32_t ctr_id = arg.ctr_id();

  if (counters.find(ctr_id) != counters.end()) {
    std::cerr << this->name() << "[" << ctr_id
              << "]: " << counters[ctr_id].pkt_count << ", "
              << counters[ctr_id].byte_count << std::endl;
    counters.erase(ctr_id);
  } else {
    return CommandFailure(EINVAL, "Unable to remove ctr");
  }
  return CommandSuccess();
}
/*----------------------------------------------------------------------------------*/
CommandResponse Counter::Init(const bess::pb::CounterArg &arg) {
  name_id = arg.name_id();
  if (name_id == "")
    return CommandFailure(EINVAL, "Invalid counter idx name");
  using AccessMode = bess::metadata::Attribute::AccessMode;
  attr_id = AddMetadataAttr(name_id, sizeof(uint32_t), AccessMode::kRead);

  return CommandSuccess();
}
/*----------------------------------------------------------------------------------*/
void Counter::ProcessBatch(Context *ctx, bess::PacketBatch *batch) {
  int cnt = batch->cnt();

  for (int i = 0; i < cnt; i++) {
    uint32_t ctr_id = get_attr<uint32_t>(this, attr_id, batch->pkts()[i]);
    // check if ctr_id is present
    if (counters.find(ctr_id) != counters.end()) {
      SessionStats s = counters[ctr_id];
      s.pkt_count += 1;
      s.byte_count += batch->pkts()[i]->total_len();
      counters.erase(ctr_id);
      counters.insert(std::pair<uint32_t, SessionStats>(ctr_id, s));
    }
  }

  RunNextModule(ctx, batch);
}
/*----------------------------------------------------------------------------------*/
std::string Counter::GetDesc() const {
  return bess::utils::Format("%zu sessions", (size_t)counters.size());
}
/*----------------------------------------------------------------------------------*/
ADD_MODULE(Counter, "counter",
           "Counts the number of packets/bytes in the UP4 pipeline")