#pragma once
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

class PowerTriggerModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    PowerTriggerModule();

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    void powerOn();
    void powerOff();
};

extern PowerTriggerModule *powerTriggerModule;