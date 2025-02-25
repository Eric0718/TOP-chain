// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/manager/xcontract_manager.h"

#include "xbase/xmem.h"
#include "xchain_upgrade/xchain_data_processor.h"
#include "xcodec/xmsgpack_codec.hpp"
#include "xcommon/xip.h"
#include "xconfig/xconfig_register.h"
#include "xdata/xblocktool.h"
#include "xdata/xcodec/xmsgpack/xelection_association_result_store_codec.hpp"
#include "xdata/xcodec/xmsgpack/xelection_result_store_codec.hpp"
#include "xdata/xcodec/xmsgpack/xstandby_result_store_codec.hpp"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xtransaction_v1.h"
#include "xmbus/xevent_store.h"
#include "xmbus/xevent_timer.h"
#include "xmetrics/xmetrics.h"
#include "xvledger/xvblock.h"
#include "xvm/manager/xcontract_address_map.h"
#include "xvm/manager/xmessage_ids.h"
#include "xvm/xsystem_contracts/deploy/xcontract_deploy.h"
#include "xvm/xsystem_contracts/tcc/xrec_proposal_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_archive_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_edge_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_fullnode_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_rec_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_zec_contract.h"
#include "xvm/xsystem_contracts/xelection/xrec/xrec_standby_pool_contract.h"
#include "xvm/xsystem_contracts/xelection/xzec/xzec_elect_consensus_group_contract.h"
#include "xvm/xsystem_contracts/xelection/xzec/xzec_group_association_contract.h"
#include "xvm/xsystem_contracts/xelection/xzec/xzec_standby_pool_contract.h"
#include "xvm/xsystem_contracts/xregistration/xrec_registration_contract.h"
#include "xvm/xsystem_contracts/xreward/xtable_reward_claiming_contract.h"
#include "xvm/xsystem_contracts/xreward/xtable_vote_contract.h"
#include "xvm/xsystem_contracts/xreward/xzec_reward_contract.h"
#include "xvm/xsystem_contracts/xreward/xzec_vote_contract.h"
#include "xvm/xsystem_contracts/xslash/xtable_statistic_info_collection_contract.h"
#include "xvm/xsystem_contracts/xslash/xzec_slash_info_contract.h"
#include "xvm/xsystem_contracts/xworkload/xzec_workload_contract_v2.h"
#include "xvm/xvm_service.h"

#include <cinttypes>

using namespace top::base;
using namespace top::mbus;
using namespace top::common;
using namespace top::vnetwork;
using namespace top::store;

NS_BEG2(top, contract)

xtop_contract_manager & xtop_contract_manager::instance() {
    static xtop_contract_manager * inst = new xtop_contract_manager();
    return *inst;
}

xtop_contract_manager::~xtop_contract_manager() {
    XMETRICS_COUNTER_INCREMENT("xvm_contract_manager_counter", -1);
    clear();
}

#define XREGISTER_CONTRACT(CONTRACT_TYPE, CONTRACT_ADDRESS_STR, CONTRACT_NETWORK_ID)                                                                                               \
    m_contract_register.add<CONTRACT_TYPE>(top::common::xaccount_address_t{CONTRACT_ADDRESS_STR}, CONTRACT_NETWORK_ID)

void xtop_contract_manager::instantiate_sys_contracts() {
    common::xnetwork_id_t network_id{top::config::to_chainid(XGET_CONFIG(chain_name))};
    XREGISTER_CONTRACT(top::xstake::xrec_registration_contract, sys_contract_rec_registration_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::xzec_workload_contract_v2, sys_contract_zec_workload_addr, network_id);
    XREGISTER_CONTRACT(top::xstake::xzec_vote_contract, sys_contract_zec_vote_addr, network_id);
    XREGISTER_CONTRACT(top::xstake::xzec_reward_contract, sys_contract_zec_reward_addr, network_id);
    XREGISTER_CONTRACT(top::xstake::xtable_vote_contract, sys_contract_sharding_vote_addr, network_id);
    XREGISTER_CONTRACT(top::tcc::xrec_proposal_contract, sys_contract_rec_tcc_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_elect_edge_contract_t, sys_contract_rec_elect_edge_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_elect_fullnode_contract_t, sys_contract_rec_elect_fullnode_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_elect_archive_contract_t, sys_contract_rec_elect_archive_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_elect_rec_contract_t, sys_contract_rec_elect_rec_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_elect_zec_contract_t, sys_contract_rec_elect_zec_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::rec::xrec_standby_pool_contract_t, sys_contract_rec_standby_pool_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::zec::xzec_elect_consensus_group_contract_t, sys_contract_zec_elect_consensus_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::zec::xzec_standby_pool_contract_t, sys_contract_zec_standby_pool_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::zec::xgroup_association_contract_t, sys_contract_zec_group_assoc_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::xcontract::xzec_slash_info_contract, sys_contract_zec_slash_info_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::system_contracts::reward::xtable_reward_claiming_contract_t, sys_contract_sharding_reward_claiming_addr, network_id);
    XREGISTER_CONTRACT(top::xvm::xcontract::xtable_statistic_info_collection_contract, sys_contract_sharding_statistic_info_addr, network_id);

}

#undef XREGISTER_CONTRACT

void xtop_contract_manager::setup_blockchains(xvblockstore_t * blockstore) {
    // setup all contracts' accounts, then no need
    // sync generation block at all
    for (auto const & pair : xcontract_deploy_t::instance().get_map()) {
        if (data::is_sys_sharding_contract_address(pair.first)) {
            for (auto i = 0; i < enum_vbucket_has_tables_count; i++) {
                auto addr = data::make_address_by_prefix_and_subaddr(pair.first.value(), i);
                register_contract_cluster_address(pair.first, addr);
                setup_chain(addr, blockstore);
            }
        } else {
            register_contract_cluster_address(pair.first, pair.first);
            setup_chain(pair.first, blockstore);
        }
    }
}

void xtop_contract_manager::register_address() {
    for (auto const & pair : xcontract_deploy_t::instance().get_map()) {
        if (data::is_sys_sharding_contract_address(pair.first)) {
            for (auto i = 0; i < enum_vbucket_has_tables_count; i++) {
                auto addr = data::make_address_by_prefix_and_subaddr(pair.first.value(), i);
                register_contract_cluster_address(pair.first, addr);
            }
        } else {
            register_contract_cluster_address(pair.first, pair.first);
        }
    }
}

xcontract_base * xtop_contract_manager::get_contract(common::xaccount_address_t const & address) {
    xcontract_base * pc{};
    if (data::is_sys_contract_address(address)) {  // by cluster address
        m_rwlock.lock_read();
        auto it = m_contract_inst_map.find(address);
        if (it != m_contract_inst_map.end()) {
            pc = it->second;
        }
        m_rwlock.release_read();
    } else {
        pc = m_contract_register.get_contract(address);  // by name
    }
    return pc;
}

void xtop_contract_manager::install_monitors(observer_ptr<xmessage_bus_face_t> const & bus,
                                             observer_ptr<vnetwork::xmessage_callback_hub_t> const & msg_callback_hub,
                                             observer_ptr<xstore_face_t> const & store,
                                             xobject_ptr_t<store::xsyncvstore_t> const & syncstore) {
    msg_callback_hub->register_message_ready_notify([this, nid = msg_callback_hub->network_id(), bus_ptr = bus.get()](xvnode_address_t const &, xmessage_t const & msg, std::uint64_t const) {
        if (msg.id() == xmessage_block_broadcast_id) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)msg.payload().data(), msg.payload().size());
            base::xauto_ptr<xvblock_t> block(data::xblock_t::full_block_read_from(stream));
            if (block != nullptr) {
                auto store_block = [this, bus_ptr](top::base::xcall_t & call,const int32_t thread_id, const uint64_t time_now_ms)->bool
                {
                    base::xvblock_t * block = dynamic_cast<base::xvblock_t*>(call.get_param1().get_object());
                    //xcontract_manager_t * contract_manager = reinterpret_cast<contract::xcontract_manager_t*>(call.get_param2().get_object());
                    if (block->get_account() == sys_contract_beacon_timer_addr) {
                        base::xvaccount_t _vaccount(sys_contract_beacon_timer_addr);
                        base::xblock_vector blocks = this->m_syncstore->get_vblockstore()->load_block_object(_vaccount, block->get_height());
                        if (!blocks.get_vector().empty()) {
                            xdbg("xtop_contract_manager::install_monitors received repeat tc block=%s", block->dump().c_str());
                            return true;
                        }
                    }
                    bool succ = this->m_syncstore->store_block(block);
                    xinfo("contract manager sees: received broadcast block=%s save %s",
                          block->dump().c_str(),
                          succ ? "SUCC" : "FAIL");
                    if (block->get_account() == sys_contract_beacon_timer_addr) {
                        if (!succ) {
                            // don't know why save failed, verify...
                            block->reset_block_flags();
                            auto succ = this->m_syncstore->get_vcertauth()->verify_muti_sign(block) == base::enum_vcert_auth_result::enum_successful;
                            if (succ) {
                                block->set_block_flag(base::enum_xvblock_flag_authenticated);
                            }
                        }
                        if (succ) {
                            auto event_ptr = make_object_ptr<xevent_chain_timer_t>(block);
                            bus_ptr->push_event(event_ptr);
                            xdbg("[xtop_contract_manager::install_monitors] push event");
                            XMETRICS_GAUGE_SET_VALUE(metrics::clock_received_height, block->get_height());
                        }
                    }
                    return true;
                };

                base::xcall_t asyn_call(store_block, (base::xobject_t*)block.get());
                get_thread()->send_call(asyn_call);
            } else {
                xerror("contract_manager xmessage_block_broadcast_id: recv invalid data");
            }
        }
    });

    bus->add_listener(mbus::xevent_major_type_store, std::bind(&xtop_contract_manager::push_event, this, std::placeholders::_1));
    bus->add_listener(mbus::xevent_major_type_chain_timer, std::bind(&xtop_contract_manager::push_event, this, std::placeholders::_1));
}

void xtop_contract_manager::clear() {
    for (auto & pair : m_map) {
        delete pair.second;
    }
    m_map.clear();

    m_contract_inst_map.clear();
}

bool xtop_contract_manager::filter_event(const xevent_ptr_t & e) {
    switch (e->major_type) {
    case xevent_major_type_store:
        return e->minor_type == xevent_store_t::type_block_committed;
    case xevent_major_type_vnode:
        return true;
    case xevent_major_type_chain_timer:
        return true;
    default:
        return false;
    }
}

void xtop_contract_manager::after_event_pushed(const xevent_ptr_t & e) {
    if (e->major_type == xevent_major_type_vnode) {
        ((xevent_vnode_t *)e.get())->wait();  // wait till event processed
    }
}

void xtop_contract_manager::process_event(const xevent_ptr_t & e) {
    xinfo("xtop_contract_manager::process_event %d", e->major_type);
    switch (e->major_type) {
    case xevent_major_type_store:
    case xevent_major_type_chain_timer:
        do_on_block(e);
        break;
    case xevent_major_type_vnode: {
        auto event = dynamic_xobject_ptr_cast<xevent_vnode_t>(e);
        if (event->destory) {
            do_destory_vnode(event);
        } else {
            do_new_vnode(event);
        }
        event->notify();
        break;
    }
    default:
        xwarn("xtop_contract_manager::process_event won't process event type %d", e->major_type);
        break;
    }
}

void xtop_contract_manager::do_destory_vnode(const xevent_vnode_ptr_t & e) {
    xrole_map_t * rm{};
    for (auto it = m_map.begin(), last = m_map.end(); it != last;) {
        rm = it->second;
        auto it1 = rm->find(e->driver.get());
        if (it1 != rm->end()) {
            delete it1->second;
            rm->erase(it1);
        }

        if (rm->empty()) {
            delete rm;
            it = m_map.erase(it);
        } else {
            ++it;
        }
    }
}

void xtop_contract_manager::do_on_block(const xevent_ptr_t & e) {
    if (e->major_type == xevent_major_type_chain_timer) {
        assert(dynamic_xobject_ptr_cast<xevent_chain_timer_t>(e) != nullptr);
        auto const event = dynamic_xobject_ptr_cast<xevent_chain_timer_t>(e);
        auto height = event->time_block->get_height();
        if (height <= m_latest_timer) {
            xdbg("[xtop_contract_manager::do_on_block] ignore timer block %" PRIu64 ", record timer height %" PRIu64, height, m_latest_timer);
            return;  // ignore
        }

        m_latest_timer = height;  // record
        xdbg("[xtop_contract_manager::do_on_block] on timer block=%s, map size %d", event->time_block->dump().c_str(), m_map.size());
        for (auto & pair : m_map) { // m_map : std::unordered_map<common::xaccount_address_t, xrole_map_t *>
            for (auto & pr : *(pair.second)) {  // using xrole_map_t = std::unordered_map<xvnetwork_driver_face_t *, xrole_context_t *>;
                pr.second->on_block_timer(e);
            }
        }
    } else if (e->major_type == xevent_major_type_store && e->minor_type == xevent_store_t::type_block_committed) {
        // TODO(jimmy) check if need process firstly
        xevent_store_block_committed_ptr_t store_event = dynamic_xobject_ptr_cast<xevent_store_block_committed_t>(e);
        if (store_event->blk_level != base::enum_xvblock_level_table) {  // only broadcast table
            return;
        }

        auto block = mbus::extract_block_from(store_event, metrics::blockstore_access_from_mbus_contract_db_on_block); // load mini-block firstly
        if (block == nullptr) {  // should not happen
            assert(false);
            return;
        }
        xdbg("[xtop_contract_manager::do_on_block] on block to db, block=%s, map size %d", block->dump().c_str(), m_map.size());
        bool event_broadcasted{false};
        for (auto & pair : m_map) { // m_map : std::unordered_map<common::xaccount_address_t, xrole_map_t *>
            // auto const & account_address = top::get<common::xaccount_address_t const>(pair);
            for (auto & pr : *(pair.second)) {  // using xrole_map_t = std::unordered_map<xvnetwork_driver_face_t *, xrole_context_t *>;
                pr.second->on_block_to_db(block, event_broadcasted);
            }
        }
    }
}

void xtop_contract_manager::do_new_vnode(const xevent_vnode_ptr_t & e) {
    common::xnode_type_t type = e->driver->type();
    xdbg("[xtop_contract_manager::do_new_vnode] node type : %s", common::to_string(type).c_str());
    add_role_contexts_by_type(e, type, false);

    if (common::has<common::xnode_type_t::consensus_auditor>(type)) {
        xdbg("[xtop_contract_manager::do_new_vnode] add all sharding contracts' rcs");
        add_role_contexts_by_type(e, common::xnode_type_t::consensus_validator, true);  // add sharding rcs, but disable broadcasts
    }
}

void xtop_contract_manager::add_role_contexts_by_type(const xevent_vnode_ptr_t & e, common::xnode_type_t type, bool disable_broadcasts) {
    for (auto & pair : xcontract_deploy_t::instance().get_map()) {  // pair : std::pair<common::xaccount_address_t const, xcontract_info_t *>
        auto * contract_info_ptr = top::get<xcontract_info_t *>(pair);

        if (common::has(type, contract_info_ptr->roles)) {
            xrole_map_t * m{};
            auto it = m_map.find(contract_info_ptr->address);
            if (it != m_map.end()) {
                m = it->second;
            }
            if (m == nullptr) {
                m = new xrole_map_t();
                m_map[contract_info_ptr->address] = m;
            }
            auto cloned_contract_info_ptr = new xcontract_info_t(*contract_info_ptr);
            if (disable_broadcasts) {
                cloned_contract_info_ptr->broadcast_types = common::xnode_type_t::invalid;  // disable broadcasts
            }
            auto prc = new xrole_context_t(m_store, m_syncstore, e->unit_service, e->driver, cloned_contract_info_ptr);
            add_to_map(*m, prc, e->driver.get());
        }
    }
}

void xtop_contract_manager::add_to_map(xrole_map_t & m, xrole_context_t * rc, xvnetwork_driver_face_t * driver) {
    if (!m.empty()) {  // using xrole_map_t = std::unordered_map<xvnetwork_driver_face_t *, xrole_context_t *>;
        for (auto & pair : m) {
            auto const & sharding_address = top::get<xvnetwork_driver_face_t * const>(pair)->address().sharding_address();

            if (sharding_address == driver->address().sharding_address()) {
                delete top::get<xrole_context_t *>(pair);
                m.erase(pair.first);
                break;
            }
        }
    }

    m[driver] = rc;
}

void xtop_contract_manager::init(observer_ptr<xstore_face_t> const & store,
                                 xobject_ptr_t<store::xsyncvstore_t> const& syncstore) {
    m_store = store;
    m_syncstore = make_observer(syncstore.get());
}

void xtop_contract_manager::setup_chain(common::xaccount_address_t const & contract_cluster_address, xvblockstore_t * blockstore) {
    assert(contract_cluster_address.has_value());

    if (blockstore->exist_genesis_block(contract_cluster_address.value())) {
        xdbg("xtop_contract_manager::setup_chain blockchain account %s genesis block exist", contract_cluster_address.c_str());
        return;
    }
    xdbg("xtop_contract_manager::setup_chain blockchain account %s genesis block not exist", contract_cluster_address.c_str());

    xtransaction_ptr_t tx = make_object_ptr<xtransaction_v1_t>();
    data::xproperty_asset asset_out{0};
    tx->make_tx_run_contract(asset_out, "setup", "");
    tx->set_same_source_target_address(contract_cluster_address.value());
    tx->set_digest();
    tx->set_len();

    xobject_ptr_t<base::xvbstate_t> bstate = make_object_ptr<base::xvbstate_t>(contract_cluster_address.value(), (uint64_t)0, (uint64_t)0, std::string(), std::string(), (uint64_t)0, (uint32_t)0, (uint16_t)0);
    xaccount_ptr_t unitstate = std::make_shared<xunit_bstate_t>(bstate.get());
    xaccount_context_t ac(unitstate, m_store.get());

    xvm::xvm_service s;
    s.deal_transaction(tx, &ac);

    store::xtransaction_result_t result;
    ac.get_transaction_result(result);

    base::xauto_ptr<base::xvblock_t> block(data::xblocktool_t::create_genesis_lightunit(contract_cluster_address.value(), tx, result));
    xassert(block);

    base::xvaccount_t _vaddr(block->get_account());
    // m_blockstore->delete_block(_vaddr, genesis_block.get());  // delete default genesis block
    auto ret = blockstore->store_block(_vaddr, block.get());
    if (!ret) {
        xerror("xtop_contract_manager::setup_chain %s genesis block fail", contract_cluster_address.c_str());
        return;
    }
    xdbg("[xtop_contract_manager::setup_chain] setup %s, %s", contract_cluster_address.c_str(), ret ? "SUCC" : "FAIL");
}

void xtop_contract_manager::register_contract_cluster_address(common::xaccount_address_t const & address, common::xaccount_address_t const & cluster_address) {
    m_rwlock.lock_write();
    auto it = m_contract_inst_map.find(cluster_address);
    if (it == m_contract_inst_map.end()) {
        xcontract_base * pc = m_contract_register.get_contract(address);
        if (pc != nullptr) {
            m_contract_inst_map[cluster_address] = pc;
        }
    }
    m_rwlock.release_write();
}

base::xvnodesrv_t * xtop_contract_manager::m_nodesvr_ptr = NULL;

int32_t xtop_contract_manager::get_account_from_xip(const xvip2_t & target_node, std::string & target_addr) {
    assert(m_nodesvr_ptr != NULL);
    base::xauto_ptr<base::xvnode_t> leader_node = m_nodesvr_ptr->get_node(target_node);
    if (leader_node == nullptr) {
        xwarn("[xtop_contract_manager::get_account_from_xip] fail-found target node(%" PRIu64 " : %" PRIu64 ")", target_node.high_addr, target_node.low_addr);
        return -1;
    }
    target_addr = leader_node->get_account();
    return 0;
}

base::xvnodesrv_t * xtop_contract_manager::get_node_service() const noexcept {
    return m_nodesvr_ptr;
}

static void get_election_result_property_data(observer_ptr<store::xstore_face_t const> store,
                                              common::xaccount_address_t const & contract_address,
                                              xjson_format_t const json_format,
                                              xJson::Value & json) {
    assert(contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr} ||      // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr} ||      // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr} ||     // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr} ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||
           contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr});

    std::string serialized_value{};
    for (auto const & property_name : data::election::get_property_name_by_addr(contract_address)) {
        serialized_value.clear();
        if (store->string_get(contract_address.value(), property_name, serialized_value) == 0 && !serialized_value.empty()) {
            auto election_result_store = codec::msgpack_decode<data::election::xelection_result_store_t>({std::begin(serialized_value), std::end(serialized_value)});
            for (auto const & election_network_result_info : election_result_store) {
                auto const network_id = top::get<common::xnetwork_id_t const>(election_network_result_info);
                auto const & election_network_result = top::get<data::election::xelection_network_result_t>(election_network_result_info);
                xJson::Value jn;
                for (auto const & election_result_info : election_network_result) {
                    auto const node_type = top::get<common::xnode_type_t const>(election_result_info);
                    std::string node_type_str = to_string(node_type);
                    if (node_type_str.back() == '.') {
                        node_type_str = node_type_str.substr(0, node_type_str.size() - 1);
                    }
                    auto const & election_result = top::get<data::election::xelection_result_t>(election_result_info);

                    for (auto const & election_cluster_result_info : election_result) {
                        // auto const & cluster_id = top::get<common::xcluster_id_t const>(election_cluster_result_info);
                        auto const & election_cluster_result = top::get<data::election::xelection_cluster_result_t>(election_cluster_result_info);

                        for (auto const & group_result_info : election_cluster_result) {
                            auto const & group_id = top::get<common::xgroup_id_t const>(group_result_info);
                            auto const & election_group_result = top::get<data::election::xelection_group_result_t>(group_result_info);

                            for (auto const & node_info : election_group_result) {
                                auto const & node_id = top::get<data::election::xelection_info_bundle_t>(node_info).node_id();
                                if (node_id.empty()) {
                                    continue;
                                }
                                auto const & election_info = top::get<data::election::xelection_info_bundle_t>(node_info).election_info();

                                switch (json_format) {
                                case xjson_format_t::simple:
                                    jn.append(node_id.to_string());
                                    break;

                                case xjson_format_t::detail: {
                                    xJson::Value j;
                                    j["group_id"] = group_id.value();
                                    j["stake"] = static_cast<xJson::UInt64>(election_info.stake);
                                    j["round"] = static_cast<xJson::UInt64>(election_group_result.group_version().value());
                                    jn[node_id.to_string()].append(j);

                                    break;
                                }

                                default:
                                    assert(false);
                                    break;
                                }
                            }
                        }
                    }

                    json[common::to_string(network_id)][node_type_str] = jn;
                }
            }
        }
    }
}

static void get_election_result_property_data(observer_ptr<store::xstore_face_t const> store,
                                              common::xaccount_address_t const & contract_address,
                                              std::string const & property_name,
                                              xjson_format_t const json_format,
                                              bool compatible_mode,
                                              xJson::Value & json) {
    assert(contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr}       ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr}       ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr}      ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr}   ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr});

    std::string serialized_value{};
    if (store->string_get(contract_address.value(), property_name, serialized_value) == 0 && !serialized_value.empty()) {
        auto election_result_store = codec::msgpack_decode<data::election::xelection_result_store_t>({std::begin(serialized_value), std::end(serialized_value)});
        for (auto const & election_network_result_info : election_result_store) {
            auto const network_id = top::get<common::xnetwork_id_t const>(election_network_result_info);
            auto const & election_network_result = top::get<data::election::xelection_network_result_t>(election_network_result_info);

            for (auto const & election_result_info : election_network_result) {
                auto const node_type = top::get<common::xnode_type_t const>(election_result_info);
                std::string node_type_str = compatible_mode ? common::to_presentation_string_compatible(node_type) : common::to_presentation_string(node_type);
                auto const & election_result = top::get<data::election::xelection_result_t>(election_result_info);
                xJson::Value jn;
                for (auto const & election_cluster_result_info : election_result) {
                    // auto const & cluster_id = top::get<common::xcluster_id_t const>(election_cluster_result_info);
                    auto const & election_cluster_result = top::get<data::election::xelection_cluster_result_t>(election_cluster_result_info);

                    for (auto const & group_result_info : election_cluster_result) {
                        auto const & group_id = top::get<common::xgroup_id_t const>(group_result_info);
                        auto const & election_group_result = top::get<data::election::xelection_group_result_t>(group_result_info);

                        for (auto const & node_info : election_group_result) {
                            auto const & node_id = top::get<data::election::xelection_info_bundle_t>(node_info).node_id();
                            if (node_id.empty()) {
                                continue;
                            }
                            auto const & election_info = top::get<data::election::xelection_info_bundle_t>(node_info).election_info();

                            switch (json_format) {
                            case xjson_format_t::simple:
                                json.append(node_id.to_string());
                                break;

                            case xjson_format_t::detail: {
                                xJson::Value j;
                                j["group_id"] = group_id.value();
                                j["stake"] = static_cast<xJson::UInt64>(election_info.stake);
                                j["round"] = static_cast<xJson::UInt64>(election_group_result.group_version().value());
                                j["public_key"] = election_info.consensus_public_key.to_string();
                                j["genesis"] = election_info.genesis ? "true" : "false";
                                j["miner_type"] = common::to_string(election_info.miner_type);
                                jn[node_id.to_string()].append(j);

                                break;
                            }

                            default:
                                assert(false);
                                break;
                            }
                        }
                    }
                }

                switch (json_format) {
                case xjson_format_t::simple:
                    break;
                case xjson_format_t::detail:
                    json[node_type_str] = jn;
                    json["chain_id"] = common::to_string(network_id);
                    break;
                default:
                    assert(false);
                    break;
                }
            }
        }
    }
}

static void get_election_result_property_data(const xaccount_ptr_t unitstate,
                                              common::xaccount_address_t const & contract_address,
                                              std::string const & property_name,
                                              xjson_format_t const json_format,
                                              bool compatible_mode,
                                              xJson::Value & json) {
    assert(contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr} ||      // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr} ||      // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr} ||     // NOLINT
           contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr} ||  // NOLINT
           contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||
           contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr});

    std::string serialized_value{};
    if (unitstate->string_get(property_name, serialized_value) && !serialized_value.empty()) {
        auto election_result_store = codec::msgpack_decode<data::election::xelection_result_store_t>({std::begin(serialized_value), std::end(serialized_value)});
        for (auto const & election_network_result_info : election_result_store) {
            auto const network_id = top::get<common::xnetwork_id_t const>(election_network_result_info);
            auto const & election_network_result = top::get<data::election::xelection_network_result_t>(election_network_result_info);

            for (auto const & election_result_info : election_network_result) {
                auto const node_type = top::get<common::xnode_type_t const>(election_result_info);
                std::string node_type_str = compatible_mode ? common::to_presentation_string_compatible(node_type) : common::to_presentation_string(node_type);
                auto const & election_result = top::get<data::election::xelection_result_t>(election_result_info);
                xJson::Value jn;
                for (auto const & election_cluster_result_info : election_result) {
                    // auto const & cluster_id = top::get<common::xcluster_id_t const>(election_cluster_result_info);
                    auto const & election_cluster_result = top::get<data::election::xelection_cluster_result_t>(election_cluster_result_info);

                    for (auto const & group_result_info : election_cluster_result) {
                        auto const & group_id = top::get<common::xgroup_id_t const>(group_result_info);
                        auto const & election_group_result = top::get<data::election::xelection_group_result_t>(group_result_info);

                        for (auto const & node_info : election_group_result) {
                            auto const & node_id = top::get<data::election::xelection_info_bundle_t>(node_info).node_id();
                            if (node_id.empty()) {
                                continue;
                            }
                            auto const & election_info = top::get<data::election::xelection_info_bundle_t>(node_info).election_info();

                            switch (json_format) {
                            case xjson_format_t::simple:
                                json.append(node_id.to_string());
                                break;

                            case xjson_format_t::detail: {
                                xJson::Value j;
                                j["group_id"] = group_id.value();
                                j["stake"] = static_cast<xJson::UInt64>(election_info.stake);
                                j["round"] = static_cast<xJson::UInt64>(election_group_result.group_version().value());
                                j["public_key"] = election_info.consensus_public_key.to_string();
                                j["genesis"] = election_info.genesis ? "true" : "false";
                                j["miner_type"] = common::to_string(election_info.miner_type);
                                jn[node_id.to_string()].append(j);

                                break;
                            }

                            default:
                                assert(false);
                                break;
                            }
                        }
                    }
                }

                switch (json_format) {
                case xjson_format_t::simple:
                    break;
                case xjson_format_t::detail:
                    json[node_type_str] = jn;
                    json["chain_id"] = common::to_string(network_id);
                    break;
                default:
                    assert(false);
                    break;
                }
            }
        }
    }
}

static void get_rec_standby_pool_property_data(observer_ptr<store::xstore_face_t const> store,
                                               common::xaccount_address_t const & contract_address,
                                               std::string const property_name,
                                               xjson_format_t const json_format,
                                               bool compatible_mode,
                                               xJson::Value & json) {
    assert(property_name == XPROPERTY_CONTRACT_STANDBYS_KEY);
    assert(contract_address == common::xaccount_address_t{sys_contract_rec_standby_pool_addr});
    std::string serialized_value{};
    if (store->string_get(contract_address.value(), property_name, serialized_value) == 0 && !serialized_value.empty()) {
        auto const & standby_result_store = codec::msgpack_decode<data::election::xstandby_result_store_t>({std::begin(serialized_value), std::end(serialized_value)});
        for (auto const & standby_network_result_info : standby_result_store) {
            auto const network_id = top::get<common::xnetwork_id_t const>(standby_network_result_info);
            auto const & standby_network_result = top::get<data::election::xstandby_network_storage_result_t>(standby_network_result_info).all_network_result();
            for (auto const & standby_result_info : standby_network_result) {
                auto const node_type = top::get<common::xnode_type_t const>(standby_result_info);
                std::string node_type_str = compatible_mode ? common::to_presentation_string_compatible(node_type) : common::to_presentation_string(node_type);
                auto const standby_result = top::get<data::election::xstandby_result_t>(standby_result_info);
                for (auto const & node_info : standby_result) {
                    auto const & node_id = top::get<common::xnode_id_t const>(node_info);
                    switch (json_format) {
                    case xjson_format_t::simple:
                        json[node_type_str].append(node_id.value());
                        break;
                    case xjson_format_t::detail: {
                        xJson::Value j;
                        auto const & standby_node_info = top::get<data::election::xstandby_node_info_t>(node_info);
                        j["consensus_public_key"] = standby_node_info.consensus_public_key.to_string();
                        j["node_id"] = node_id.value();
                        j["stake"] = static_cast<xJson::UInt64>(standby_node_info.stake(node_type));
                        j["is_genesis_node"] = std::string{standby_node_info.genesis ? "true" : "false"};
                        j["program_version"] = standby_node_info.program_version;
                        j["miner_type"] = common::to_string(standby_node_info.miner_type);
                        json[node_type_str].append(j);
                        break;
                    }
                    default:
                        assert(false);
                        break;
                    }
                }
            }
            json["activated_state"] = std::string{standby_result_store.result_of(network_id).activated_state() ? "activated" : "not activated"};
        }
    }
}

static void get_zec_standby_pool_property_data(observer_ptr<store::xstore_face_t const> store,
                                               common::xaccount_address_t const & contract_address,
                                               std::string const property_name,
                                               xJson::Value & json) {
    assert(property_name == XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_LOGIC_TIME || property_name == XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_BLOCK_HEIGHT);
    assert(contract_address == common::xaccount_address_t{sys_contract_zec_standby_pool_addr});
    std::string value{};
    if (store->string_get(contract_address.value(), property_name, value) == 0) {
        json[property_name] = value;
    }
}

static void get_association_result_property_data(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    assert(contract_address == xaccount_address_t{sys_contract_zec_group_assoc_addr});
    std::string serialized_value{};
    if (store->string_get(contract_address.value(), property_name, serialized_value) == 0 && !serialized_value.empty()) {
        auto const & association_result_store =
            codec::msgpack_decode<data::election::xelection_association_result_store_t>({std::begin(serialized_value), std::end(serialized_value)});
        for (auto const & election_association_result : association_result_store) {
            for (auto const & association_result : election_association_result.second) {
                json[association_result.second.to_string()].append(association_result.first.value());
            }
        }
    }
}

static void get_genesis_stage(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::string value;
    if ( store->string_get(contract_address.value(), property_name, value) != 0 ) return;
    data::system_contract::xactivation_record record;
    if (!value.empty()) {
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)value.data(), static_cast<uint32_t>(value.size())};
        record.serialize_from(stream);
    }

    json["activated"] = (xJson::Int)record.activated;
    json["activation_time"] = (xJson::UInt64)record.activation_time;
}

static void get_rec_nodes_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if ( store->map_copy_get(contract_address.value(), property_name, nodes) != 0 ) return;
    for (auto m : nodes) {
        data::system_contract::xreg_node_info reg_node_info;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)m.second.data(), m.second.size());
        reg_node_info.serialize_from(stream);
        xJson::Value j;
        j["account_addr"] = reg_node_info.m_account.value();
        j["node_deposit"] = static_cast<unsigned long long>(reg_node_info.m_account_mortgage);
        if (reg_node_info.genesis()) {
            j["registered_node_type"] = std::string{"advance,validator,edge,archive"};
        } else {
            j["registered_node_type"] = common::to_string(reg_node_info.miner_type());
        }
        j["vote_amount"] = static_cast<unsigned long long>(reg_node_info.m_vote_amount);
        {
            auto credit = static_cast<double>(reg_node_info.m_auditor_credit_numerator) / reg_node_info.m_auditor_credit_denominator;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << credit;
            j["auditor_credit"] = ss.str();
        }
        {
            auto credit = static_cast<double>(reg_node_info.m_validator_credit_numerator) / reg_node_info.m_validator_credit_denominator;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << credit;
            j["validator_credit"] = ss.str();
        }
        j["dividend_ratio"] = reg_node_info.m_support_ratio_numerator * 100 / reg_node_info.m_support_ratio_denominator;
        // j["m_stake"] = static_cast<unsigned long long>(reg_node_info.m_stake);
        j["auditor_stake"] = static_cast<unsigned long long>(reg_node_info.get_auditor_stake());
        j["validator_stake"] = static_cast<unsigned long long>(reg_node_info.get_validator_stake());
        j["rec_stake"] = static_cast<unsigned long long>(reg_node_info.rec_stake());
        j["zec_stake"] = static_cast<unsigned long long>(reg_node_info.zec_stake());
        std::string network_ids;
        for (auto const & net_id : reg_node_info.m_network_ids) {
            network_ids += net_id.to_string() + ' ';
        }
        j["network_id"] = network_ids;
        j["nodename"] = reg_node_info.nickname;
        j["node_sign_key"] = reg_node_info.consensus_public_key.to_string();
        json[m.first] = j;
    }
}

static void get_zec_workload_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> workloads;
    if (store->map_copy_get(contract_address.value(), property_name, workloads) != 0) return;
    for (auto m : workloads) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        data::system_contract::cluster_workload_t workload;
        workload.serialize_from(stream);
        xJson::Value jn;
        jn["cluster_total_workload"] = workload.cluster_total_workload;
        auto const & key_str = m.first;
        common::xcluster_address_t cluster;
        xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
        key_stream >> cluster;
        XSTD_PRINT("--------------------cluster: %s, size: %u\n", cluster.group_id().to_string().c_str(), static_cast<uint32_t>(workload.m_leader_count.size()));
        for (auto node : workload.m_leader_count) {
            jn[node.first] = node.second;
        }
        json[cluster.group_id().to_string()] = jn;
    }
}

static void get_zec_tasks_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> tasks;
    if (store->map_copy_get(contract_address.value(), property_name, tasks) != 0) return;
    for (auto m : tasks) {
        auto const & detail = m.second;
        if (detail.empty())
            continue;

        data::system_contract::xreward_dispatch_task task;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        task.serialize_from(stream);

        xJson::Value jv;
        xJson::Value jvn;
        jv["task_id"] = m.first;
        jv["onchain_timer_round"] = (xJson::UInt64)task.onchain_timer_round;
        jv["contract"] = task.contract;
        jv["action"] = task.action;
        if (task.action == data::system_contract::XREWARD_CLAIMING_ADD_NODE_REWARD || task.action == data::system_contract::XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD) {
            base::xstream_t stream_params{xcontext_t::instance(), (uint8_t *)task.params.data(), static_cast<uint32_t>(task.params.size())};
            uint64_t onchain_timer_round;
            std::map<std::string, ::uint128_t> rewards;
            stream_params >> onchain_timer_round;
            stream_params >> rewards;

            for (auto v : rewards) {
                jvn[v.first] = std::to_string(static_cast<uint64_t>(v.second / data::system_contract::REWARD_PRECISION)) + std::string(".")
                    + std::to_string(static_cast<uint32_t>(v.second % data::system_contract::REWARD_PRECISION));
            }

            jv["rewards"] = jvn;
        } else if (task.action == data::system_contract::XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            base::xstream_t seo_stream(base::xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                jvn[issue.first] = std::to_string(issue.second);
            }
        }
        json.append(jv);
    }
}

static void get_zec_votes(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> votes;
    if (store->map_copy_get(contract_address.value(), property_name, votes) != 0) return;
    std::map<std::string, std::string> votes_table;
    for (auto m : votes) {
        auto detail = m.second;
        if (!detail.empty()) {
            votes_table.clear();
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
            stream >> votes_table;
        }

        xJson::Value jv;
        xJson::Value jvn;
        for (auto v : votes_table) {
            jvn[v.first] = (xJson::UInt64)base::xstring_utl::touint64(v.second);
        }
        jv["vote_infos"] = jvn;
        json[m.first] = jv;
    }
}

static void get_table_votes(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> votes;
    store->map_copy_get(contract_address.value(), property_name, votes);
    std::map<std::string, uint64_t> vote_info;
    for (auto m : votes) {
        auto detail = m.second;
        if (!detail.empty()) {
            vote_info.clear();
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
            stream >> vote_info;
        }

        xJson::Value jv;
        xJson::Value jvn;
        for (auto v : vote_info) {
            jvn[v.first] = (xJson::UInt64)v.second;
        }
        jv["vote_infos"] = jvn;
        json[m.first] = jv;
    }
}

static void get_voter_dividend(observer_ptr<store::xstore_face_t> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    xdbg("[get_voter_dividend] contract_address: %s, property_name: %s", contract_address.c_str(), property_name.c_str());
    std::map<std::string, std::string> voter_dividends;

    //store->map_copy_get(contract_address.value(), property_name, voter_dividends);
    uint64_t blockchain_height = store->get_blockchain_height(contract_address.value());
    if(xsuccess != store->get_map_property(contract_address.value(), blockchain_height, property_name, voter_dividends)){
        return ;
    }

    for (auto m : voter_dividends) {
        data::system_contract::xreward_record record;
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        record.serialize_from(stream);

        xJson::Value jv;
        jv["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(record.accumulated / data::system_contract::REWARD_PRECISION);
        jv["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.accumulated % data::system_contract::REWARD_PRECISION);
        jv["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(record.unclaimed / data::system_contract::REWARD_PRECISION);
        jv["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.unclaimed % data::system_contract::REWARD_PRECISION);
        jv["last_claim_time"] = (xJson::UInt64)record.last_claim_time;
        jv["issue_time"] = (xJson::UInt64)record.issue_time;

        xJson::Value jvm;
        int no = 0;
        for (auto n : record.node_rewards) {
            xJson::Value jvn;
            jvn["account_addr"] = n.account;
            jvn["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(n.accumulated / data::system_contract::REWARD_PRECISION);
            jvn["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(n.accumulated % data::system_contract::REWARD_PRECISION);
            jvn["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(n.unclaimed / data::system_contract::REWARD_PRECISION);
            jvn["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(n.unclaimed % data::system_contract::REWARD_PRECISION);
            jvn["last_claim_time"] = (xJson::UInt64)n.last_claim_time;
            jvn["issue_time"] = (xJson::UInt64)n.issue_time;
            jvm[no++] = jvn;
        }
        jv["node_dividend"] = jvm;

        json[m.first] = jv;
    }
}

static void get_node_reward(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> node_rewards;
    store->map_copy_get(contract_address.value(), property_name, node_rewards);
    data::system_contract::xreward_node_record record;
    for (auto m : node_rewards) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        record.serialize_from(stream);

        xJson::Value jv;
        jv["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(record.m_accumulated / data::system_contract::REWARD_PRECISION);
        jv["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.m_accumulated % data::system_contract::REWARD_PRECISION);
        jv["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(record.m_unclaimed / data::system_contract::REWARD_PRECISION);
        jv["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.m_unclaimed % data::system_contract::REWARD_PRECISION);
        jv["last_claim_time"] = (xJson::UInt64)record.m_last_claim_time;
        jv["issue_time"] = (xJson::UInt64)record.m_issue_time;

        json[m.first] = jv;
    }
}

static void get_reward_detail(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::string serialized_value{};
    if (!unitstate->string_get(property_name, serialized_value) || serialized_value.empty()) {
        xdbg("[get_reward_detail] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xissue_detail issue_detail;
    issue_detail.from_string(serialized_value);
    xJson::Value jv;
    jv["onchain_timer_round"] = (xJson::UInt64)issue_detail.onchain_timer_round;
    jv["zec_vote_contract_height"] = (xJson::UInt64)issue_detail.m_zec_vote_contract_height;
    jv["zec_workload_contract_height"] = (xJson::UInt64)issue_detail.m_zec_workload_contract_height;
    jv["zec_reward_contract_height"] = (xJson::UInt64)issue_detail.m_zec_reward_contract_height;
    jv["edge_reward_ratio"] = issue_detail.m_edge_reward_ratio;
    jv["archive_reward_ratio"] = issue_detail.m_archive_reward_ratio;
    jv["validator_reward_ratio"] = issue_detail.m_validator_reward_ratio;
    jv["auditor_reward_ratio"] = issue_detail.m_auditor_reward_ratio;
    jv["vote_reward_ratio"] = issue_detail.m_vote_reward_ratio;
    jv["governance_reward_ratio"] = issue_detail.m_governance_reward_ratio;
    jv["validator_group_count"] = (xJson::UInt)issue_detail.m_validator_group_count;
    jv["auditor_group_count"] = (xJson::UInt)issue_detail.m_auditor_group_count;
    xJson::Value jr;
    for (auto const & node_reward : issue_detail.m_node_rewards) {
        std::stringstream ss;
        ss << "edge_reward: " << static_cast<uint64_t>(node_reward.second.m_edge_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_edge_reward % data::system_contract::REWARD_PRECISION)
           << ", archive_reward: " << static_cast<uint64_t>(node_reward.second.m_archive_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_archive_reward % data::system_contract::REWARD_PRECISION)
           << ", validator_reward: " << static_cast<uint64_t>(node_reward.second.m_validator_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_validator_reward % data::system_contract::REWARD_PRECISION)
           << ", auditor_reward: " << static_cast<uint64_t>(node_reward.second.m_auditor_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_auditor_reward % data::system_contract::REWARD_PRECISION)
           << ", voter_reward: " << static_cast<uint64_t>(node_reward.second.m_vote_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_vote_reward % data::system_contract::REWARD_PRECISION)
           << ", self_reward: " << static_cast<uint64_t>(node_reward.second.m_self_reward / data::system_contract::REWARD_PRECISION) << "." << std::setw(6) << std::setfill('0')
           << static_cast<uint32_t>(node_reward.second.m_self_reward % data::system_contract::REWARD_PRECISION);
        jr[node_reward.first] = ss.str();
    }
    jv["node_rewards"] = jr;
    json = jv;
}

static void get_refunds(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> refunds;
    store->map_copy_get(contract_address.value(), property_name, refunds);
    data::system_contract::xrefund_info refund;
    for (auto m : refunds) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        refund.serialize_from(stream);

        xJson::Value jv;
        jv["refund_amount"] = (xJson::UInt64)refund.refund_amount;
        jv["create_time"] = (xJson::UInt64)refund.create_time;

        json[m.first] = jv;
    }
}

static void get_accumulated_issuance_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> issuances;
    if ( store->map_copy_get(contract_address.value(), property_name, issuances) != 0 ) return;
    xJson::Value jv;
    for (auto const & m : issuances) {
        jv[m.first] = m.second;
    }
    json["accumulated"] = jv;
}

static void get_accumulated_issuance_yearly_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::string value;
    if ( store->string_get(contract_address.value(), property_name, value) != 0 ) return;
    data::system_contract::xaccumulated_reward_record record;
    xstream_t stream(xcontext_t::instance(), (uint8_t *)value.c_str(), (uint32_t)value.size());
    record.serialize_from(stream);
    json["last_issuance_time"]                    = (xJson::UInt64)record.last_issuance_time;
    json["issued_until_last_year_end"]            = (xJson::UInt64)static_cast<uint64_t>(record.issued_until_last_year_end / data::system_contract::REWARD_PRECISION);
    json["issued_until_last_year_end_decimals"]   = (xJson::UInt)static_cast<uint32_t>(record.issued_until_last_year_end % data::system_contract::REWARD_PRECISION);
}

static void get_unqualified_node_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if ( store->map_copy_get(contract_address.value(), property_name, nodes) != 0 ) return;
    xunqualified_node_info_t summarize_info;
    for (auto const & m : nodes) {
        auto detail = m.second;
        if (!detail.empty()) {
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), (uint32_t)detail.size()};
            summarize_info.serialize_from(stream);
        }

        xJson::Value jvn;
        xJson::Value jvn_auditor;
        for (auto const & v : summarize_info.auditor_info) {
            xJson::Value auditor_info;
            auditor_info["vote_num"] = v.second.block_count;
            auditor_info["subset_num"] = v.second.subset_count;
            jvn_auditor[v.first.value()] = auditor_info;
        }

        xJson::Value jvn_validator;
        for (auto const & v : summarize_info.validator_info) {
            xJson::Value validator_info;
            validator_info["vote_num"] = v.second.block_count;
            validator_info["subset_num"] = v.second.subset_count;
            jvn_validator[v.first.value()] = validator_info;
        }

        jvn["auditor"] = jvn_auditor;
        jvn["validator"] = jvn_validator;
        json["unqualified_node"] = jvn;
    }
}

static void get_unqualified_slash_info_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if ( store->map_copy_get(contract_address.value(), property_name, nodes) != 0 ) return;
    data::system_contract::xslash_info s_info;
    for (auto const & m : nodes) {
        auto detail = m.second;
        if (!detail.empty()) {
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), (uint32_t)detail.size()};
            s_info.serialize_from(stream);
        }

        xJson::Value jvn;
        jvn["punish_time"] = (xJson::UInt64)s_info.m_punish_time;
        jvn["staking_lock_time"] = (xJson::UInt64)s_info.m_staking_lock_time;
        jvn["punish_staking"] = (xJson::UInt)s_info.m_punish_staking;

        json[m.first] = jvn;
    }
}

static void get_proposal_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> proposals;
    if ( store->map_copy_get(contract_address.value(), property_name, proposals) != 0 ) return;
     tcc::proposal_info pi;
    for (auto m : proposals) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        pi.deserialize(stream);

        xJson::Value jv;
        jv["proposal_id"] = pi.proposal_id;
        jv["proposal_type"] = (xJson::UInt)pi.type;
        jv["target"] = pi.parameter;
        jv["value"] = pi.new_value;
        // jv["modification_description"] = pi.modification_description;
        jv["proposer_account_addr"] = pi.proposal_client_address;
        jv["proposal_deposit"] = (xJson::UInt64)(pi.deposit);
        jv["effective_timer_height"] = (xJson::UInt64)(pi.effective_timer_height);
        jv["priority"] = pi.priority;
        // jv["cosigning_status"] = pi.cosigning_status;
        jv["voting_status"] = pi.voting_status;
        jv["expire_time"] = (xJson::UInt64)(pi.end_time);

        json["proposal_id_" + m.first] = jv;
    }
}


static void get_proposal_voting_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> proposal_votings;
    if ( store->map_copy_get(contract_address.value(), property_name, proposal_votings) != 0 ) return;
    for (auto m : proposal_votings) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        std::map<std::string, bool> nodes;
        stream >> nodes;
        xJson::Value jn;
        for (auto node : nodes) {
            jn[node.first] = node.second;
        }
        json[m.first] = jn;
    }
}

static void get_other_map(observer_ptr<store::xstore_face_t const> store,
                                                 common::xaccount_address_t const & contract_address,
                                                 std::string const & property_name,
                                                 xJson::Value & json) {
    std::map<std::string, std::string> unknowns;
    if ( store->map_copy_get(contract_address.value(), property_name, unknowns) != 0 ) return;
    for (auto m : unknowns) {
        json[m.first] = m.second;
    }
}


static void get_sharding_statistic_contract_property(std::string const & sharding_contract_addr,
                                                    std::string const & property_name,
                                                    uint64_t const height,
                                                    observer_ptr<store::xstore_face_t> store,
                                                    xJson::Value & json,
                                                    std::error_code & ec
                                                    ) {

    assert(!ec);
    assert(store != nullptr);

    std::map<std::string, std::string> result;
    if (property_name == data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY) {
        auto error = store->get_map_property(sharding_contract_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_fail_to_get_block;
            return;
        }

#if defined(DEBUG)
        xdbg("get_table_statistic_contract_property #131, height: %" PRIu64 ", map size: %zu", height, result.size());
        for (auto const & p : result) {
            xdbg("get_table_statistic_contract_property, key: %s; value: %s", p.first.c_str(), p.second.c_str());
        }
#endif

        auto const it = result.find("UNQUALIFIED_NODE");
        if (it == std::end(result)) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

        auto const & value = it->second;
        if (value.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        xunqualified_node_info_t data;
        base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(value.data())), static_cast<uint32_t>(value.size()) };
        try {
            data.serialize_from(stream);
        } catch (top::error::xtop_error_t const & eh) {
            ec = eh.code();
            return;
        } catch (enum_xerror_code const errc) {
            ec = errc;
            return;
        }

        for (auto const & auditor_data : data.auditor_info) {
            xJson::Value v;
            auto const & unqualified_data = top::get<xnode_vote_percent_t>(auditor_data);

            v["account"] = top::get<common::xaccount_address_t const>(auditor_data).value();
            v["block_count"] = unqualified_data.block_count;
            v["subset_count"] = unqualified_data.subset_count;

            json[property_name]["auditor"].append(v);
        }

        for (auto const & validator_data : data.validator_info) {
            xJson::Value v;
            auto const & unqualified_data = top::get<xnode_vote_percent_t>(validator_data);

            v["account"] = top::get<common::xaccount_address_t const>(validator_data).value();
            v["block_count"] = unqualified_data.block_count;
            v["subset_count"] = unqualified_data.subset_count;

            json[property_name]["validator"].append(v);
        }
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY) {
        auto error = store->get_map_property(sharding_contract_addr, height, property_name, result);
        if (error) {
            return;
        }

#if defined(DEBUG)
        xdbg("get_table_statistic_contract_property #134, height: %" PRIu64 ", map size: %zu", height, result.size());
        for (auto const & p : result) {
            xdbg("get_table_statistic_contract_property, key: %s; value: %s", p.first.c_str(), p.second.c_str());
        }
#endif

        auto it = result.find("FULLTABLE_NUM");
        if (it == std::end(result)) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

        auto& value = it->second;
        if (value.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        uint32_t summarize_fulltableblock_count = xstring_utl::touint32(value);
        json[property_name]["fulltableblock_count"] = summarize_fulltableblock_count;


        it = result.find("FULLTABLE_HEIGHT");
        if (it == std::end(result)) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

        value = it->second;
        if (value.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        uint32_t summarize_fulltableblock_height = xstring_utl::touint32(value);
        json[property_name]["fulltableblock_height"] = summarize_fulltableblock_height;


    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY) {
        auto error = store->get_map_property(sharding_contract_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_fail_to_get_block;
            return;
        }

#if defined(DEBUG)
        for (auto const & p : result) {
            xdbg("table_statistic_contract: key : %s; value size %zu", p.first.c_str(), p.second.size());
        }
#endif
        if (result.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        for (auto const & m : result) {
            xJson::Value jm;
            auto const & detail = m.second;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(detail.data())), static_cast<uint32_t>(detail.size()) };
            data::system_contract::cluster_workload_t workload;
            try {
                workload.serialize_from(stream);
            } catch (top::error::xtop_error_t const & eh) {
                ec = eh.code();
                return;
            } catch (enum_xerror_code const errc) {
                ec = errc;
                return;
            }
            {
                xJson::Value jn;
                jn["cluster_total_workload"] = workload.cluster_total_workload;
                auto const & key_str = workload.cluster_id;
                common::xcluster_address_t cluster;
                base::xstream_t key_stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(key_str.data())), static_cast<uint32_t>(key_str.size()) };
                key_stream >> cluster;
                for (auto const & node : workload.m_leader_count) {
                    jn[node.first] = node.second;
                }
                jm[cluster.group_id().to_string()] = jn;
            }
            json["auditor_workload"].append(jm);
        }

    }
}

static void get_zec_slash_contract_property(std::string const & property_name,
                                            uint64_t const height,
                                            observer_ptr<store::xstore_face_t> store,
                                            xJson::Value & json,
                                            std::error_code & ec) {
    assert(!ec);
    assert(store != nullptr);

    std::map<std::string, std::string> result;
    if (property_name == data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY) {
        auto error = store->get_map_property(sys_contract_zec_slash_info_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_fail_to_get_block;
            return;
        }

#if defined(DEBUG)
        xdbg("get_zec_slash_contract_property #131, height: %" PRIu64 ", map size: %zu", height, result.size());
        for (auto const & p : result) {
            xdbg("get_zec_slash_contract_property, key: %s; value: %s", p.first.c_str(), p.second.c_str());
        }
#endif

        auto const it = result.find("UNQUALIFIED_NODE");
        if (it == std::end(result)) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

        auto const & value = it->second;
        if (value.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        xunqualified_node_info_t data;
        base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(value.data())), static_cast<uint32_t>(value.size()) };
        try {
            data.serialize_from(stream);
        } catch (top::error::xtop_error_t const & eh) {
            ec = eh.code();
            return;
        } catch (enum_xerror_code const errc) {
            ec = errc;
            return;
        }

        for (auto const & auditor_data : data.auditor_info) {
            xJson::Value v;
            auto const & unqualified_data = top::get<xnode_vote_percent_t>(auditor_data);

            v["account"] = top::get<common::xaccount_address_t const>(auditor_data).value();
            v["block_count"] = unqualified_data.block_count;
            v["subset_count"] = unqualified_data.subset_count;

            json[property_name]["auditor"].append(v);
        }

        for (auto const & validator_data : data.validator_info) {
            xJson::Value v;
            auto const & unqualified_data = top::get<xnode_vote_percent_t>(validator_data);

            v["account"] = top::get<common::xaccount_address_t const>(validator_data).value();
            v["block_count"] = unqualified_data.block_count;
            v["subset_count"] = unqualified_data.subset_count;

            json[property_name]["validator"].append(v);
        }
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY) {
        auto error = store->get_map_property(sys_contract_zec_slash_info_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

#if defined(DEBUG)
        xdbg("get_zec_slash_contract_property #133, height: %" PRIu64 ", map size: %zu", height, result.size());
        for (auto const & p : result) {
            xdbg("get_zec_slash_contract_property, key: %s; value: %s", p.first.c_str(), p.second.c_str());
        }
#endif

        auto const it = result.find("TABLEBLOCK_NUM");
        if (it == std::end(result)) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_missing;
            return;
        }

        auto const & value = it->second;
        if (value.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        uint32_t summarize_tableblock_count;
        base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(value.data())), static_cast<uint32_t>(value.size()) };
        try {
            stream >> summarize_tableblock_count;
        } catch (top::error::xtop_error_t const & eh) {
            ec = eh.code();
            return;
        } catch (enum_xerror_code const errc) {
            ec = errc;
            return;
        }

        json[property_name]["accumulated_tableblock_count"] = summarize_tableblock_count;


        // all table height
        xJson::Value v;
        for (auto table_id = 0; table_id < enum_vledger_const::enum_vbucket_has_tables_count; ++table_id) {
            auto const it = result.find(std::to_string(table_id));

            if (it == std::end(result)) {
                continue;
            }

            auto const & value = it->second;
            if (value.empty()) {
                continue;
            }

            uint64_t height;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(value.data())), static_cast<uint32_t>(value.size()) };
            try {
                stream >> height;
            } catch (top::error::xtop_error_t const & eh) {
                ec = eh.code();
                return;
            } catch (enum_xerror_code const errc) {
                ec = errc;
                return;
            }

            v[std::to_string(table_id)] = (xJson::UInt64)height;

        }

        json[property_name]["table_heights"].append(v);

    }
}

static void get_zec_reward_contract_property(std::string const & property_name,
                                            uint64_t const height,
                                            observer_ptr<store::xstore_face_t> store,
                                            xJson::Value & json,
                                            std::error_code & ec) {
    assert(!ec);
    assert(store != nullptr);

    std::map<std::string, std::string> result;

    if (property_name == data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY) {
        auto error = store->get_map_property(sys_contract_zec_reward_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_fail_to_get_block;
            return;
        }

#if defined(DEBUG)
        for (auto const & p : result) {
            xdbg("sys_contract_zec_reward_addr: key : %s; value size %zu", p.first.c_str(), p.second.size());
        }
#endif
        if (result.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        for (auto const & m : result) {
            xJson::Value jm;
            auto const & detail = m.second;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(detail.data())), static_cast<uint32_t>(detail.size()) };
            data::system_contract::cluster_workload_t workload;
            try {
                workload.serialize_from(stream);
            } catch (top::error::xtop_error_t const & eh) {
                ec = eh.code();
                return;
            } catch (enum_xerror_code const errc) {
                ec = errc;
                return;
            }
            {
                xJson::Value jn;
                jn["cluster_total_workload"] = workload.cluster_total_workload;
                auto const & key_str = workload.cluster_id;
                common::xcluster_address_t cluster;
                base::xstream_t key_stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(key_str.data())), static_cast<uint32_t>(key_str.size()) };
                key_stream >> cluster;
                for (auto const & node : workload.m_leader_count) {
                    jn[node.first] = node.second;
                }
                jm[cluster.group_id().to_string()] = jn;
            }
            json["auditor_workload"].append(jm);
        }
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY) {
        auto error = store->get_map_property(sys_contract_zec_reward_addr, height, property_name, result);
        if (error) {
            ec = xvm::enum_xvm_error_code::query_contract_data_fail_to_get_block;
            return;
        }

#if defined(DEBUG)
        for (auto const & p : result) {
            xdbg("sys_contract_zec_reward_addr: key : %s; value size %zu", p.first.c_str(), p.second.size());
        }
#endif
        if (result.empty()) {
            ec = xvm::enum_xvm_error_code::query_contract_data_property_empty;
            return;
        }

        for (auto const & m : result) {
            xJson::Value jm;
            auto const & detail = m.second;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(detail.data())), static_cast<uint32_t>(detail.size()) };
            data::system_contract::cluster_workload_t workload;
            try {
                workload.serialize_from(stream);
            } catch (top::error::xtop_error_t const & eh) {
                ec = eh.code();
                return;
            } catch (enum_xerror_code const errc) {
                ec = errc;
                return;
            }
            {
                xJson::Value jn;
                jn["cluster_total_workload"] = workload.cluster_total_workload;
                auto const & key_str = workload.cluster_id;
                common::xcluster_address_t cluster;
                base::xstream_t key_stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(key_str.data())), static_cast<uint32_t>(key_str.size()) };
                key_stream >> cluster;
                for (auto const & node : workload.m_leader_count) {
                    jn[node.first] = node.second;
                }
                jm[cluster.group_id().to_string()] = jn;
            }
            json["validator_workload"].append(jm);
        }
    }
}

void xtop_contract_manager::get_contract_data(common::xaccount_address_t const & contract_address, xjson_format_t const json_format, bool compatible_mode, xJson::Value & json) const {
    if (contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr} ||     // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr} ||  // NOLINT
        contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||
        contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr}) {
        return get_election_result_property_data(m_store, contract_address, json_format, json);
    } else if (contract_address == xaccount_address_t{sys_contract_rec_standby_pool_addr}) {
        return get_rec_standby_pool_property_data(m_store, contract_address, XPROPERTY_CONTRACT_STANDBYS_KEY, json_format, compatible_mode, json);
    } else if (contract_address == xaccount_address_t{sys_contract_zec_standby_pool_addr}) {
        get_zec_standby_pool_property_data(m_store, contract_address, XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_BLOCK_HEIGHT, json);
        get_zec_standby_pool_property_data(m_store, contract_address, XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_LOGIC_TIME, json);
        return;
    } else if (contract_address == xaccount_address_t{sys_contract_zec_group_assoc_addr}) {
        return get_association_result_property_data(m_store, contract_address, XPROPERTY_CONTRACT_GROUP_ASSOC_KEY, json);
    }
}

void xtop_contract_manager::get_contract_data(common::xaccount_address_t const & contract_address,
                                              std::uint64_t const height,
                                              xjson_format_t const json_format,
                                              xJson::Value & json,
                                              std::error_code & ec) const {
    assert(!ec);
    if (contract_address.value().find(sys_contract_sharding_statistic_info_addr) != std::string::npos ) {
        std::error_code internal_ec;
        get_sharding_statistic_contract_property(contract_address.value(), data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, height, m_store, json, internal_ec);
        if (internal_ec) {
            xdbg("table_statistic_contract, get data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY failed");
            ec = internal_ec;
            internal_ec.clear();
        }
        get_sharding_statistic_contract_property(contract_address.value(), data::system_contract::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, height, m_store, json, internal_ec);
        if (internal_ec) {
            xdbg("table_statistic_contract, get data::system_contract::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY failed");
            ec = internal_ec;
            internal_ec.clear();
        }
        get_sharding_statistic_contract_property(contract_address.value(), data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY, height, m_store, json, internal_ec);
        if (internal_ec) {
            xdbg("table_statistic_contract, get data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY failed");
            ec = internal_ec;
            internal_ec.clear();
        }

    } else if (contract_address == xaccount_address_t{ sys_contract_zec_slash_info_addr }) {
        std::error_code internal_ec;
        get_zec_slash_contract_property(data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, height, m_store, json, internal_ec);
        if (internal_ec) {
            xdbg("get data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY failed");
            ec = internal_ec;
            internal_ec.clear();
        }
        get_zec_slash_contract_property(data::system_contract::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, height, m_store, json, internal_ec);
        if (internal_ec && !ec) {
            ec = internal_ec;
        }
    } else if (contract_address == xaccount_address_t{ sys_contract_zec_reward_addr }) {
        std::error_code internal_ec;
        get_zec_reward_contract_property(data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY, height, m_store, json, internal_ec);
        if (internal_ec) {
            xdbg("get data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY failed");
            ec = internal_ec;
            internal_ec.clear();
        }
        get_zec_reward_contract_property(data::system_contract::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, height, m_store, json, internal_ec);
        if (internal_ec && !ec) {
            xdbg("get data::system_contract::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY failed");
            ec = internal_ec;
        }
    }
}

void xtop_contract_manager::get_contract_data(common::xaccount_address_t const & contract_address,
                                              std::string const & property_name,
                                              xjson_format_t const json_format,
                                              bool compatible_mode,
                                              xJson::Value & json) const {
    if (contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr} ||     // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr} ||  // NOLINT
        contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||
        contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr}) {
        if (contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} && property_name == XPROPERTY_CONTRACT_ELECTION_EXECUTED_KEY) {
            std::string res;
            m_store->string_get(contract_address.value(), property_name, res);
            json[property_name] = res;
            return;
        }
        return get_election_result_property_data(m_store, contract_address, property_name, json_format, compatible_mode, json);
    } else if (contract_address == xaccount_address_t{sys_contract_rec_standby_pool_addr}) {
        return get_rec_standby_pool_property_data(m_store, contract_address, property_name, json_format, compatible_mode, json);
    } else if (contract_address == xaccount_address_t{sys_contract_zec_standby_pool_addr}) {
        return get_zec_standby_pool_property_data(m_store, contract_address, property_name, json);
    } else if (contract_address == xaccount_address_t{sys_contract_zec_group_assoc_addr}) {
        return get_association_result_property_data(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY) {
        return get_genesis_stage(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_REG_KEY) {
        return get_rec_nodes_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY || property_name == data::system_contract::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY) {
        return get_zec_workload_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_TASK_KEY) {
        return get_zec_tasks_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_TICKETS_KEY) {
        return get_zec_votes(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY1 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY2 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY3 ||
        property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY4) {
        return get_table_votes(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY1 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY2 ||
        property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY3 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY4) {
        return get_voter_dividend(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_REFUND_KEY) {
        return get_refunds(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE) {
        return get_accumulated_issuance_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY) {
        return get_unqualified_node_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY) {
        return get_accumulated_issuance_yearly_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_SLASH_INFO_KEY) {
        return get_unqualified_slash_info_map(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_NODE_REWARD_KEY) {
        return get_node_reward(m_store, contract_address, property_name, json);
    } else if (property_name == PROPOSAL_MAP_ID) {
        return get_proposal_map(m_store, contract_address, property_name, json);
    } else if (property_name == VOTE_MAP_ID) {
        return get_proposal_voting_map(m_store, contract_address, property_name, json);
    }
}

static void get_rec_nodes_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if (!unitstate->map_get(property_name, nodes) || nodes.empty()) {
        xdbg("[get_rec_nodes_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    for (auto m : nodes) {
        data::system_contract::xreg_node_info reg_node_info;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)m.second.data(), m.second.size());
        reg_node_info.serialize_from(stream);
        xJson::Value j;
        j["account_addr"] = reg_node_info.m_account.value();
        j["node_deposit"] = static_cast<unsigned long long>(reg_node_info.m_account_mortgage);
        if (reg_node_info.genesis()) {
            j["registered_node_type"] = std::string{"advance,validator,edge"};
        } else {
            j["registered_node_type"] = common::to_string(reg_node_info.miner_type());
        }
        j["miner_type"] = common::to_string(reg_node_info.miner_type());
        j["vote_amount"] = static_cast<unsigned long long>(reg_node_info.m_vote_amount);
        {
            auto credit = static_cast<double>(reg_node_info.m_auditor_credit_numerator) / reg_node_info.m_auditor_credit_denominator;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << credit;
            j["auditor_credit"] = ss.str();
        }
        {
            auto credit = static_cast<double>(reg_node_info.m_validator_credit_numerator) / reg_node_info.m_validator_credit_denominator;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6) << credit;
            j["validator_credit"] = ss.str();
        }
        j["dividend_ratio"] = reg_node_info.m_support_ratio_numerator * 100 / reg_node_info.m_support_ratio_denominator;
        // j["m_stake"] = static_cast<unsigned long long>(reg_node_info.m_stake);
        j["auditor_stake"] = static_cast<unsigned long long>(reg_node_info.get_auditor_stake());
        j["validator_stake"] = static_cast<unsigned long long>(reg_node_info.get_validator_stake());
        j["rec_stake"] = static_cast<unsigned long long>(reg_node_info.rec_stake());
        j["zec_stake"] = static_cast<unsigned long long>(reg_node_info.zec_stake());
        std::string network_ids;
        for (auto const & net_id : reg_node_info.m_network_ids) {
            network_ids += net_id.to_string() + ' ';
        }
        j["network_id"] = network_ids;
        j["nodename"] = reg_node_info.nickname;
        j["node_sign_key"] = reg_node_info.consensus_public_key.to_string();
        json[m.first] = j;
    }
}

static void get_unqualified_slash_info_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if (!unitstate->map_get(property_name, nodes) || nodes.empty()) {
        xdbg("[get_unqualified_slash_info_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xslash_info s_info;
    for (auto const & m : nodes) {
        auto detail = m.second;
        if (!detail.empty()) {
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), (uint32_t)detail.size()};
            s_info.serialize_from(stream);
        }

        xJson::Value jvn;
        jvn["punish_time"] = (xJson::UInt64)s_info.m_punish_time;
        jvn["staking_lock_time"] = (xJson::UInt64)s_info.m_staking_lock_time;
        jvn["punish_staking"] = (xJson::UInt)s_info.m_punish_staking;

        json[m.first] = jvn;
    }
}

static void get_unqualified_node_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> nodes;
    if (!unitstate->map_get(property_name, nodes) || nodes.empty()) {
        xdbg("[get_unqualified_slash_info_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    xunqualified_node_info_t summarize_info;
    for (auto const & m : nodes) {
        auto detail = m.second;
        if (!detail.empty()) {
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), (uint32_t)detail.size()};
            summarize_info.serialize_from(stream);
        }

        xJson::Value jvn;
        xJson::Value jvn_auditor;
        for (auto const & v : summarize_info.auditor_info) {
            xJson::Value auditor_info;
            auditor_info["vote_num"] = v.second.block_count;
            auditor_info["subset_num"] = v.second.subset_count;
            jvn_auditor[v.first.value()] = auditor_info;
        }

        xJson::Value jvn_validator;
        for (auto const & v : summarize_info.validator_info) {
            xJson::Value validator_info;
            validator_info["vote_num"] = v.second.block_count;
            validator_info["subset_num"] = v.second.subset_count;
            jvn_validator[v.first.value()] = validator_info;
        }

        jvn["auditor"] = jvn_auditor;
        jvn["validator"] = jvn_validator;
        json["unqualified_node"] = jvn;
    }
}


static void get_tableblock_num(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> tablenum_map;
    if (!unitstate->map_get(property_name, tablenum_map) || tablenum_map.empty()) {
        xdbg("[get_tableblock_num] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }


    // all table height
    xJson::Value v;
    for (auto const& item: tablenum_map) {
        if (item.first == "TABLEBLOCK_NUM") {
            uint32_t summarize_tableblock_count;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(item.second.data())), static_cast<uint32_t>(item.second.size()) };
            try {
                stream >> summarize_tableblock_count;
            } catch (...) {
                xdbg("[get_tableblock_num] deserialize height error");
                return;
            }
            json[property_name][item.first] = summarize_tableblock_count;
        } else {

            uint64_t height;
            base::xstream_t stream{ base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(item.second.data())), static_cast<uint32_t>(item.second.size()) };
            try {
                stream >> height;
            } catch (...) {
                xdbg("[get_tableblock_num] deserialize height error");
                return;
            }

            if (height != 0) {
                v[item.first] = (xJson::UInt64)height;
            }
        }
    }

    json[property_name]["table_heights"].append(v);


}

static void get_refunds(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> refunds;
    if (!unitstate->map_get(property_name, refunds) || refunds.empty()) {
        xdbg("[get_refunds] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xrefund_info refund;
    for (auto m : refunds) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        refund.serialize_from(stream);

        xJson::Value jv;
        jv["refund_amount"] = (xJson::UInt64)refund.refund_amount;
        jv["create_time"] = (xJson::UInt64)refund.create_time;

        json[m.first] = jv;
    }
}

static void get_accumulated_issuance_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> issuances;
    if (!unitstate->map_get(property_name, issuances) || issuances.empty()) {
        xdbg("[get_accumulated_issuance_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    xJson::Value jv;
    for (auto const & m : issuances) {
        jv[m.first] = m.second;
    }
    json["accumulated"] = jv;
}

static void get_accumulated_issuance_yearly_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::string value;
    if (!unitstate->string_get(property_name, value) || value.empty()) {
        xdbg("[get_accumulated_issuance_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xaccumulated_reward_record record;
    xstream_t stream(xcontext_t::instance(), (uint8_t *)value.c_str(), (uint32_t)value.size());
    record.serialize_from(stream);
    json["last_issuance_time"]                    = (xJson::UInt64)record.last_issuance_time;
    json["issued_until_last_year_end"]            = (xJson::UInt64)static_cast<uint64_t>(record.issued_until_last_year_end / data::system_contract::REWARD_PRECISION);
    json["issued_until_last_year_end_decimals"]   = (xJson::UInt)static_cast<uint32_t>(record.issued_until_last_year_end % data::system_contract::REWARD_PRECISION);
}

static void get_zec_tasks_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> tasks;
    if (!unitstate->map_get(property_name, tasks) || tasks.empty()) {
        xdbg("[get_zec_tasks_map] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    for (auto m : tasks) {
        auto const & detail = m.second;
        if (detail.empty())
            continue;

        data::system_contract::xreward_dispatch_task task;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        task.serialize_from(stream);

        xJson::Value jv;
        xJson::Value jvn;
        jv["task_id"] = m.first;
        jv["onchain_timer_round"] = (xJson::UInt64)task.onchain_timer_round;
        jv["contract"] = task.contract;
        jv["action"] = task.action;
        if (task.action == data::system_contract::XREWARD_CLAIMING_ADD_NODE_REWARD || task.action == data::system_contract::XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD) {
            base::xstream_t stream_params{xcontext_t::instance(), (uint8_t *)task.params.data(), static_cast<uint32_t>(task.params.size())};
            uint64_t onchain_timer_round;
            std::map<std::string, ::uint128_t> rewards;
            stream_params >> onchain_timer_round;
            stream_params >> rewards;

            for (auto v : rewards) {
                jvn[v.first] = std::to_string(static_cast<uint64_t>(v.second / data::system_contract::REWARD_PRECISION)) + std::string(".")
                    + std::to_string(static_cast<uint32_t>(v.second % data::system_contract::REWARD_PRECISION));
            }

            jv["rewards"] = jvn;
        } else if (task.action == data::system_contract::XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            base::xstream_t seo_stream(base::xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                jvn[issue.first] = std::to_string(issue.second);
            }
        }
        json.append(jv);
    }
}

static void get_genesis_stage(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::string value;
    if (!unitstate->string_get(property_name, value) || value.empty()) {
        xdbg("[get_genesis_stage] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xactivation_record record;
    if (!value.empty()) {
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)value.data(), static_cast<uint32_t>(value.size())};
        record.serialize_from(stream);
    }

    json["activated"] = (xJson::Int)record.activated;
    json["activation_time"] = (xJson::UInt64)record.activation_time;
}

static void get_voter_dividend(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    xdbg("[get_voter_dividend] contract_address: %s, property_name: %s", contract_address.c_str(), property_name.c_str());
    std::map<std::string, std::string> voter_dividends;

    if (!unitstate->map_get(property_name, voter_dividends) || voter_dividends.empty()) {
        xdbg("[get_voter_dividend] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }

    for (auto m : voter_dividends) {
        data::system_contract::xreward_record record;
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        record.serialize_from(stream);

        xJson::Value jv;
        jv["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(record.accumulated / data::system_contract::REWARD_PRECISION);
        jv["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.accumulated % data::system_contract::REWARD_PRECISION);
        jv["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(record.unclaimed / data::system_contract::REWARD_PRECISION);
        jv["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.unclaimed % data::system_contract::REWARD_PRECISION);
        jv["last_claim_time"] = (xJson::UInt64)record.last_claim_time;
        jv["issue_time"] = (xJson::UInt64)record.issue_time;

        xJson::Value jvm;
        int no = 0;
        for (auto n : record.node_rewards) {
            xJson::Value jvn;
            jvn["account_addr"] = n.account;
            jvn["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(n.accumulated / data::system_contract::REWARD_PRECISION);
            jvn["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(n.accumulated % data::system_contract::REWARD_PRECISION);
            jvn["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(n.unclaimed / data::system_contract::REWARD_PRECISION);
            jvn["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(n.unclaimed % data::system_contract::REWARD_PRECISION);
            jvn["last_claim_time"] = (xJson::UInt64)n.last_claim_time;
            jvn["issue_time"] = (xJson::UInt64)n.issue_time;
            jvm[no++] = jvn;
        }
        jv["node_dividend"] = jvm;

        json[m.first] = jv;
    }
}

static void get_node_reward(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> node_rewards;
    if (!unitstate->map_get(property_name, node_rewards) || node_rewards.empty()) {
        xdbg("[get_node_reward] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    data::system_contract::xreward_node_record record;
    for (auto m : node_rewards) {
        auto detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        record.serialize_from(stream);

        xJson::Value jv;
        jv["accumulated"] = (xJson::UInt64)static_cast<uint64_t>(record.m_accumulated / data::system_contract::REWARD_PRECISION);
        jv["accumulated_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.m_accumulated % data::system_contract::REWARD_PRECISION);
        jv["unclaimed"] = (xJson::UInt64)static_cast<uint64_t>(record.m_unclaimed / data::system_contract::REWARD_PRECISION);
        jv["unclaimed_decimals"] = (xJson::UInt)static_cast<uint32_t>(record.m_unclaimed % data::system_contract::REWARD_PRECISION);
        jv["last_claim_time"] = (xJson::UInt64)record.m_last_claim_time;
        jv["issue_time"] = (xJson::UInt64)record.m_issue_time;

        json[m.first] = jv;
    }
}

static void get_zec_votes(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> votes;
    if (!unitstate->map_get(property_name, votes) || votes.empty()) {
        xdbg("[get_node_reward] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    std::map<std::string, std::string> votes_table;
    for (auto m : votes) {
        auto detail = m.second;
        if (!detail.empty()) {
            votes_table.clear();
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
            stream >> votes_table;
        }

        xJson::Value jv;
        xJson::Value jvn;
        for (auto v : votes_table) {
            jvn[v.first] = (xJson::UInt64)base::xstring_utl::touint64(v.second);
        }
        jv["vote_infos"] = jvn;
        json[m.first] = jv;
    }
}

static void get_table_votes(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> votes;
    if (!unitstate->map_get(property_name, votes) || votes.empty()) {
        xdbg("[get_node_reward] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    std::map<std::string, uint64_t> vote_info;
    for (auto m : votes) {
        auto detail = m.second;
        if (!detail.empty()) {
            vote_info.clear();
            base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
            stream >> vote_info;
        }

        xJson::Value jv;
        xJson::Value jvn;
        for (auto v : vote_info) {
            jvn[v.first] = (xJson::UInt64)v.second;
        }
        jv["vote_infos"] = jvn;
        json[m.first] = jv;
    }
}

static void get_zec_workload_map(common::xaccount_address_t const & contract_address,
                              std::string const & property_name,
                              const xaccount_ptr_t unitstate,
                              const xjson_format_t json_format,
                              xJson::Value & json) {
    std::map<std::string, std::string> workloads;
    if (!unitstate->map_get(property_name, workloads) || workloads.empty()) {
        xdbg("[get_node_reward] contract_address: %s, property_name: %s, error", contract_address.to_string().c_str(), property_name.c_str());
        return;
    }
    for (auto m : workloads) {
        auto const & group_id = m.first;
        auto const & detail = m.second;
        base::xstream_t stream{xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
        data::system_contract::cluster_workload_t workload;
        workload.serialize_from(stream);
        xJson::Value jn;
        jn["cluster_total_workload"] = workload.cluster_total_workload;
        common::xcluster_address_t cluster;
        xstream_t key_stream(xcontext_t::instance(), (uint8_t *)group_id.data(), group_id.size());
        key_stream >> cluster;
        XSTD_PRINT("--------------------cluster: %s, size: %u\n", cluster.group_id().to_string().c_str(), static_cast<uint32_t>(workload.m_leader_count.size()));
        for (auto node : workload.m_leader_count) {
            jn[node.first] = node.second;
        }
        json[cluster.group_id().to_string()] = jn;
    }
}



void xtop_contract_manager::get_contract_data(common::xaccount_address_t const & contract_address,
                                              const xaccount_ptr_t unitstate,
                                              std::string const & property_name,
                                              xjson_format_t const json_format,
                                              bool compatible_mode,
                                              xJson::Value & json) const {
    if (contract_address == xaccount_address_t{sys_contract_rec_elect_rec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_zec_addr} ||      // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_edge_addr} ||     // NOLINT
        contract_address == xaccount_address_t{sys_contract_rec_elect_archive_addr} ||  // NOLINT
        contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} ||
        contract_address == xaccount_address_t{sys_contract_rec_elect_fullnode_addr}) {
        if (contract_address == xaccount_address_t{sys_contract_zec_elect_consensus_addr} && property_name == XPROPERTY_CONTRACT_ELECTION_EXECUTED_KEY) {
            std::string res;
            m_store->string_get(contract_address.value(), property_name, res);
            json[property_name] = res;
            return;
        }
        return get_election_result_property_data(unitstate, contract_address, property_name, json_format, compatible_mode, json);
    } else if (contract_address == xaccount_address_t{sys_contract_rec_standby_pool_addr}) {
        return get_rec_standby_pool_property_data(m_store, contract_address, property_name, json_format, compatible_mode, json);
    } else if (contract_address == xaccount_address_t{sys_contract_zec_standby_pool_addr}) {
        return get_zec_standby_pool_property_data(m_store, contract_address, property_name, json);
    } else if (contract_address == xaccount_address_t{sys_contract_zec_group_assoc_addr}) {
        return get_association_result_property_data(m_store, contract_address, property_name, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY) {
        return get_genesis_stage(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_REG_KEY) {
        return get_rec_nodes_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_WORKLOAD_KEY || property_name == data::system_contract::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY) {
        return get_zec_workload_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_TASK_KEY) {
        return get_zec_tasks_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_TICKETS_KEY) {
        return get_zec_votes(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY1 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY2 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY3 ||
        property_name == data::system_contract::XPORPERTY_CONTRACT_VOTES_KEY4) {
        return get_table_votes(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY1 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY2 ||
        property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY3 || property_name == data::system_contract::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY4) {
        return get_voter_dividend(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_REFUND_KEY) {
        return get_refunds(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE) {
        return get_accumulated_issuance_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY) {
        return get_unqualified_node_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY) {
        return get_tableblock_num(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY) {
        return get_accumulated_issuance_yearly_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPROPERTY_CONTRACT_SLASH_INFO_KEY) {
        return get_unqualified_slash_info_map(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPORPERTY_CONTRACT_NODE_REWARD_KEY) {
        return get_node_reward(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == data::system_contract::XPROPERTY_REWARD_DETAIL) {
        return get_reward_detail(contract_address, property_name, unitstate, json_format, json);
    } else if (property_name == PROPOSAL_MAP_ID) {
        return get_proposal_map(m_store, contract_address, property_name, json);
    } else if (property_name == VOTE_MAP_ID) {
        return get_proposal_voting_map(m_store, contract_address, property_name, json);
    }
}

void xtop_contract_manager::get_contract_data(common::xaccount_address_t const & contract_address,
                                              std::string const & property_name,
                                              std::string const & key,
                                              xjson_format_t const json_format,
                                              xJson::Value & json) const {}

NS_END2
