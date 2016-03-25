#pragma once
#include "abstract.h"
#include <limits>

namespace network {

struct OptionsUser
{
    int sndtimeo_ = 0;
    uint32_t max_pack_size_ = 64 * 1024;
    uint32_t max_connection_ = std::numeric_limits<uint32_t>::max();
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
    std::vector<OptionsBase*> lnks_;
    OptionsBase* parent_ = nullptr;

    virtual ~OptionsBase()
    {
        if (parent_)
            parent_->Unlink(this);

        while (!lnks_.empty())
            Unlink(lnks_.back());
    }

    void Link(OptionsBase & other)
    {
        other.opt_ = this->opt_;
        lnks_.push_back(&other);
        assert(nullptr == other.parent_);
        other.parent_ = this;
        other.OnLink();
    }

    void Unlink(OptionsBase * other)
    {
        assert(other->parent_ == this);
        other->parent_ = nullptr;
        auto it = std::find(lnks_.begin(), lnks_.end(), other);
        if (lnks_.end() != it)
            lnks_.erase(it);
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
    void SetMaxConnection(uint32_t max_connection)
    {
        opt_.max_connection_ = max_connection;
        OnSetMaxConnection();
        for (auto o:lnks_)
            o->SetMaxConnection(max_connection);
    }
    virtual void OnSetConnectedCb() {}
    virtual void OnSetReceiveCb() {}
    virtual void OnSetDisconnectedCb() {}
    virtual void OnSetSndTimeout() {}
    virtual void OnSetMaxPackSize() {}
    virtual void OnSetMaxConnection() {}
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
    Drived& SetMaxConnection(uint32_t max_connection)
    {
        OptionsBase::SetMaxConnection(max_connection);
        return GetThisDrived();
    }
};

}//namespace network
