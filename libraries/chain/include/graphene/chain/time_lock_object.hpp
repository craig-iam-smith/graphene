/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

    // OBJECTS

    // All objects use the below pattern, where they ingerit abstract_object templated on the object itself

class time_lock_balance_object : public abstract_object<time_lock_balance_object> {
public:
  static const uint8_t space_id = protocol_ids;
  static const uint8_t type_id = time_lock_balance_object_type;

  // Account which owns this balance
  account_id_type owner;
  // Money currently stored in this balance
  asset amount;
  // Duration in seconds to hold withdrawals in review
  int64_t review_period_seconds;

  // Helper method to determine the asset type held by this balance
  asset_id_type asset_type() const {
    return amount.asset_id;
  }
};

class time_lock_withdrawal_object : public abstract_object<time_lock_withdrawal_object> {
public:
  static const uint8_t space_id = protocol_ids;
  static const uint8_t type_id = time_lock_withdrawal_object_type;

  // Time-lock balance this withdrawal debits from
  time_lock_balance_object balance;
  // Amount to withdraw (asset ID is balance->amount.asset_id)
  share_type withdrawal;
  // Account to receive withdrawn funds
  account_id_type recipient;
  // End of review period: time at which to finalize the transfer
  fc::time_point_sec finalize_date;
};

// INDEXES
// These UNDEFINED STRUCT NAME ARE TAGS; THEY MAKE THE MULTI_INDEX_CONTAINERS MER SELF-DOCUMENTING
    
struct by_owner;
struct by_finalize_date;

    // OK, time to define the actual index. This is a daunting statement if
    // you're not familiar with boost multi_index_containers, but there's
    // plenty of resources online to demystify how these work.
using time_lock_balance_multi_index_type = multi_index_container<
  // The index stores time_lock_balance_objects
  time_lock_balance_object,
  // Now we give a list of different "keys" we look up objects by
  indexed_by<
    // The first index is always a by_id lookup, to fetch objects by ID
    ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
    // We also define an index to fetch balances by their owner
    // We'll sort the balances by owner, then by asset, then by review period duration
    ordered_non_unique< tag<by_owner>, composite_key<
					 time_lock_balance_object,
					 member<time_lock_balance_object, account_id_type,
						&time_lock_balance_object::owner>,
					 const_mem_fun<time_lock_balance_object, asset_id_type,
						       &time_lock_balance_object::asset_type>,
					 member<time_lock_balance_object, int64_t,
						&time_lock_balance_object::review_period_seconds>
> >
>
>;

    // generic_index is part of Graphene, and is used to store blockchain
    // objects in a nulti_index_container
using time_lock_balance_index =
    generic_index<time_lock_balance_object, time_lock_balance_multi_index_type>;
using time_lock_withdrawal_multi_index_type = multi_index_container<
  time_lock_withdrawal_object,
  indexed_by<
    ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
    ordered_non_unique<tag<by_finalize_date>,
		       member<time_lock_withdrawal_object,
			      fc::time_point_sec,
			      &time_lock_withdrawal_object::finalize_date>>
    >
>;
using time_lock_withdrawal_index =
    generic_index<time_lock_withdrawal_object, time_lock_withdrawal_multi_index_type>;
} } // Namespace graphene::chain

FC_REFLECT_DERIVED(graphene::chain::time_lock_balance_object,
		   (graphene::db::object),
		   (owner)(amount)(review_period_seconds))
FC_REFLECT_DERIVED(graphene::chain::time_lock_withdrawal_object,
		   (graphene::db::object),
		   (balance)(withdrawal)(recipient)(finalize_date))
