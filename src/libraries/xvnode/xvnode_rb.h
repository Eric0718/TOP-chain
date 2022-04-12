// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbase/xobject_ptr.h"
#include "xbasic/xmemory.hpp"
#include "xbasic/xtimer_driver_fwd.h"
#include "xchain_timer/xchain_timer.h"
#include "xdata/xchain_param.h"
#include "xdata/xtransaction_cache.h"
#include "xelect_net/include/elect_main.h"
#include "xelection/xcache/xgroup_element.h"
#include "xgrpc_mgr/xgrpc_mgr.h"
#include "xmbus/xevent_reg_holder.hpp"
#include "xmbus/xmessage_bus.h"
#include "xrouter/xrouter.h"
#include "xrpc/xrpc_init.h"
#include "xstore/xstore_face.h"
#include "xsync/xsync_object.h"
#include "xtxpool_service_v2/xtxpool_service_face.h"
#include "xtxpool_v2/xtxpool_face.h"
#include "xtxstore/xtxstore_face.h"
#include "xunit_service/xcons_face.h"
#include "xvnetwork/xvnetwork_driver_face.h"
#include "xvnode/xbasic_vnode.h"
#include "xvnode/xcomponents/xblock_sniffing/xsniffer.h"
#include "xvnode/xcomponents/xprune_data/xprune_data.h"
#include "xvnode/xvnode_face.h"

#include <memory>

NS_BEG2(top, vnode)

template <top::common::xnode_type_t VNode_T> class xtop_vnode;

template <>
class xtop_vnode <top::common::xnode_type_t::rec> : public xbasic_vnode_t{
    private:
        observer_ptr<elect::ElectMain> m_elect_main;
        std::shared_ptr<election::cache::xgroup_element_t> group_info;

    public:
        void start() override;
        void fade() override;
        void stop() override;
        void synchronize() override;

        components::sniffing::xsniffer_config_t sniff_config() const override;
        xtxpool_service_v2::xtxpool_proxy_face_ptr const & txpool_proxy() const override;
        std::shared_ptr<vnetwork::xvnetwork_driver_face_t> const & vnetwork_driver() const override;

};

template <>
class xtop_vnode <top::common::xnode_type_t::zec> : public xbasic_vnode_t{
     private:
        observer_ptr<elect::ElectMain> m_elect_main; 
        std::shared_ptr<election::cache::xgroup_element_t> group_info;
    public:  
        void start() override;
        void fade() override;
        void stop() override;
        void synchronize() override;

        components::sniffing::xsniffer_config_t sniff_config() const override;
        xtxpool_service_v2::xtxpool_proxy_face_ptr const & txpool_proxy() const override;
        std::shared_ptr<vnetwork::xvnetwork_driver_face_t> const & vnetwork_driver() const override;    
};

template <>
class xtop_vnode <top::common::xnode_type_t::storage_archive> : public xbasic_vnode_t{
    public:       
        void start() override;
};

template <>
class xtop_vnode <top::common::xnode_type_t::storage_exchange> : public xbasic_vnode_t{
    public:       
        void start() override;
};

template <>
class xtop_vnode <top::common::xnode_type_t::consensus_auditor> : public xbasic_vnode_t{
    public:    
        void start() override;   
};

template <>
class xtop_vnode <top::common::xnode_type_t::consensus_validator> : public xbasic_vnode_t{
    public:      
        void start() override; 
};

template <>
class xtop_vnode <top::common::xnode_type_t::edge> : public xbasic_vnode_t{
    public:    
        void start() override;   
};

template <>
class xtop_vnode <top::common::xnode_type_t::fullnode> : public xbasic_vnode_t{
    public:     
        void start() override;  
};

template <>
class xtop_vnode <top::common::xnode_type_t::evm_eth> : public xbasic_vnode_t{
    public:  
        void start() override;     
};



/*
template <top::common::xnode_type_t VNode_T>
class xtop_vnode_zec : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_zec> {
    public:        
};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_frozen : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_frozen> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_archive : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_archive> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_auditor : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_auditor> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_validator : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_validator> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_edge : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_edge> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_fullnode : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_fullnode> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_exchange : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_exchange> {};

template <top::common::xnode_type_t VNode_T>
class xtop_vnode_evm : public xtop_vnode_base   
    : public xbasic_vnode_t
    , public std::enable_shared_from_this<xtop_vnode_evm> {};

*/
NS_END2
