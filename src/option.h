#pragma once
#include "abstract.h"

namespace network {

struct OptionsUser
{
    int sndtimeo_ = 0;
    uint32_t max_pack_size_ = 4096;
};

struct OptionsData : public OptionsUser
{
    ConnectedCb connect_cb_;
    ReceiveCb receive_cb_;
    DisconnectedCb disconnect_cb_;

    static OptionsData& DefaultOption()
    {
        static OptionsData data;
        return data;
    }
};

struct OptionsBase
{
    OptionsData opt_;
    std::list<OptionsBase*> lnks_;

    void Link(OptionsBase & other)
    {
        other.opt_ = this->opt_;
        lnks_.push_back(&other);
        other.OnLink();
    }

    void OnLink()
    {
        for (auto o:lnks_) {
            o->opt_ = this->opt_;
            o->OnLink();
        }
    }

    void SetConnectedCb(ConnectedCb cb)
    {
        opt_.connect_cb_ = cb;
        OnSetConnectedCb();
        for (auto o:lnks_)
            o->SetConnectedCb(cb);
    }
    void SetReceiveCb(ReceiveCb cb)
    {
        opt_.receive_cb_ = cb;
        OnSetReceiveCb();
        for (auto o:lnks_)
            o->SetReceiveCb(cb);
    }
    void SetDisconnectedCb(DisconnectedCb cb)
    {
        opt_.disconnect_cb_ = cb;
        OnSetDisconnectedCb();
        for (auto o:lnks_)
            o->SetDisconnectedCb(cb);
    }
    void SetSndTimeout(int sndtimeo)
    {
        opt_.sndtimeo_ = sndtimeo;
        OnSetSndTimeout();
        for (auto o:lnks_)
            o->SetSndTimeout(sndtimeo);
    }
    void SetMaxPackSize(uint32_t max_pack_size)
    {
        opt_.max_pack_size_ = max_pack_size;
        OnSetMaxPackSize();
        for (auto o:lnks_)
            o->SetMaxPackSize(max_pack_size);
    }
    virtual void OnSetConnectedCb() {}
    virtual void OnSetReceiveCb() {}
    virtual void OnSetDisconnectedCb() {}
    virtual void OnSetSndTimeout() {}
    virtual void OnSetMaxPackSize() {}
};

template <typename Drived>
struct Options : public OptionsBase
{
    Drived& GetThisDrived()
    {
        return *static_cast<Drived*>(this);
    }

    Drived& SetConnectedCb(ConnectedCb cb)
    {
        OptionsBase::SetConnectedCb(cb);
        return GetThisDrived();
    }
    Drived& SetReceiveCb(ReceiveCb cb)
    {
        OptionsBase::SetReceiveCb(cb);
        return GetThisDrived();
    }
    Drived& SetDisconnectedCb(DisconnectedCb cb)
    {
        OptionsBase::SetDisconnectedCb(cb);
        return GetThisDrived();
    }
    Drived& SetSndTimeout(int sndtimeo)
    {
        OptionsBase::SetSndTimeout(sndtimeo);
        return GetThisDrived();
    }
    Drived& SetMaxPackSize(uint32_t max_pack_size)
    {
        OptionsBase::SetMaxPackSize(max_pack_size);
        return GetThisDrived();
    }
};

}//namespace network
