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

#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/time_lock_evaluator.hpp>
#include <graphene/chain/buyback.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/internal_exceptions.hpp>
#include <graphene/chain/special_authority.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <algorithm>
/* time_lock_evaluator.cpp */
namespace graphene { namespace chain {

// This function checks all of the data in the operation in conjunction
// with the current database state, and throws an exception if anything 
// is wrong or invalid.

	void_result time_lock_create_evaluator::do_evaluate(const operation_type& o) {
		// Let d be a shorthand way of referencing the database
		const database& d = db();

		// Given an object_id id, we can fetch the object it identifies
		// with this syntax: id(d)
		// If the ID is bad, this lookup will throw an exception, which is
		// the correct behavior for an evaluator when an operation
		// references an invalid ID.
		const account_object& owner_account = o.owner(d);
		const asset_object& balance_asset = o.initial_deposit.asset_id(d);

		// NOTE: Some checks would normally be done here to verify that the
		// accounts are authorized to hold/transact in the assets being
		// manipulated. These checks are omitted in this and the other
		// evaluators, as they are not directly relevant to this contract.

		// Check that the owner account has sufficient funds to cover the
		// initial deposit.
		FC_ASSERT(d.get_balance(owner_account, balance_asset).amount >= 
			op.initial_deposit.amount,
			"Account ${a} does not have sufficient funds to cover "
			"initial deposit of ${d}.",
			("a", owner_account.name)('d', d.to_pretty_string(o.initial_deposit)));
		return void_result();
	}
// This function applies the operation to the database
	object_id_type time_lock_create_evaluator::do_apply(const operation_type& o) {
		// Debit the initial deposit from the owner
		db().adjust_balance(o.owner, -o.initial_deposit);
		const auto& balance = 
		db().create<time_lock_balance_object>([&o](time_lock_balance_object& b) {
			b.owner = o.owner;
			b.amount = o.initial_deposit;
			b.review_period_seconds = o.review_period_seconds;
		}

		// And we're done! Note that fees are all handled automatically at
		// a lower level, so we don't need to worry about them.
		return balance.id;
	}
	void_result time_lock_deposit_evaluator::do_evaluate(const operation_type& o) {
		const database& d = db();

		const time_lock_balance_object& balance = o.balance(d);
		FC_ASSERT(balance.owner == o.owner,
			"One account may not deposit finds into another "
			"account's time-locked balance.");
		// !!! following assert had assignment instead of equivalence operator
		FC_ASSERT(balance.amount.asset_id == o.deposit.asset_id,
			"Cannot deposit ${d} into a balance denominated in ${b}.",
			("d", o.deposit.asset_id(d).symbol)("b". balance.amount.asset_id(d).symbol));
		const account_object& owner = o.owner(d);
		FC_ASSERT(d.get_balance(owner, o.deposit.asset_id).amount >= o.deposit.amount,
			"Account ${a} does not have sufficient funds to make "
			"deposit of ${d}.",
			("a", owner.name)("d", d.to_pretty_string(o.deposit)));
		return void_result();
	}
	
	void_result time_lock_deposit_evaluator::do_apply(const operation_type& o) {
		db().addjust_balance(o.owner, -o.deposit);
		db().modify(o.balance(db()), [&o](time_lock_balance_object& b) {
			b.amount += o.deposit;
		}
		return void_result(); 
	}

	void_result time_lock_withdraw_evaluator::do_evaluate(const operation_type& o) {
		const database& d = db();

		const auto& balance = o.balance(d);
		FC_ASSERT(balance.owner == o.owner,
			"Refusing to allow ${a} to withdraw from ${o}'s balance",
			("a", o.owner(d).name)("o", balance.owner(d).name));
		FC_ASSERT(o.withdrawal.asset_id == balance.amount.asset_id,
			"Cannot withdraw ${w} from a balance of ${a}.",
			("w", o.withdrawal.asset_id(d).symbol)
			("a". balance.amount.asset_id(d).symbol));

		// Although we don't use these, we look them up to make sure they exist
		const auto& owner = o.owner(d);
		const auto& recipient = o.recipient(d);
		
		return void_result();
	}
	
	object_id_type time_lock_withdraw_evaluator::do_apply(const operation_type& o) {
		const auto& d = db();
		const auto& balance = o.balance(d);
		auto review_period = balance.review_period_seconds;
		const auto& withdrawal = 
			d.create<time_lock_withdrawal_object>([&k](time_lock_withdrawal_object& w)) {
			w.balance = o.balance;
			w.withdrawal = o.withdrawal;
			w.recipient = o.recipient;
			w.finalize_date = d.head_block_time() + review_period;
		}

		// We don't move any money yet; all funds stay in the time-locked
		// balance, regardless of what withdrawal are pending, until a 
		// withdrawal is completed. Then we move funds.
		return withdrawal.id;
	}
	
	void_result time_lock_abort_withdrawal_evaluator::do_evaluate(const operation_type& o) {
		const auto& d = db();
		const auto& balance = o.withdrawal(d).balance(d);

		FC_ASSERT(o.owner == balance.owner,
			"Refusing to allow ${a} to abort ${o}'s withdrawal.",
			("a", o.owner(d).name)("o", balance.owner(d).name);
		return void_result();
	}

	void_result time_lock_abort_withdrawal_evaluator::do_apply(const operation_type& o) {
		db().remove(o.withdrawal(db()));
		return void_result();
	}

	void_result time_lock_complete_withdrawal_evaluator::do_evaluate(const operation_type& o) {
		const auto& d = db();
		const auto& withdrawal = o.withdrawal(d);
		const auto& balance = withdrawal.balance(d);

		FC_ASSERT(d.head_block_time() >= withdrawal.finalize_date,
			"Refusing to complete withdrawal before review period ends.");
		FC_ASSERT(balance.amount >= o.amount,
			"Cannot withdraw ${a} from a balance of ${b} ",
			("a", d.to_pretty_string(o.amount))("b", d.to_pretty_string(balance.amount)));
		FC_ASSERT(o.acting_account == balance.owner ||
					o.acting_account == withdrawal.recipient,
			"Only the owner or recipient of a time-locked ",
			"withdrawal may complete the withdrawal.");
		FC_ASSERT(o.recipient == withdrawal.recipient,
			"Refusing to complete withdrawal with incorrect recipient.");
		FC_ASSERT(o.amount.amount == withdrawal.withdrawal,
			"Refusing to complete withdrawal with incorrect amount.");
		FC_ASSERT(o.amount.asset_id == balance.amount.asset_id,
			"Refusing to complete withdrawal with incorrect asset.");
		return void_result();
		
	}

	void_result time_lock_complete_withdrawal_evaluator::do_apply(const operation_type& o) {
		const auto& d = db();
		const auto& withdrawal = o.withdrawal(d);
		const auto& balance = withdrawal.balance(d);

		d.modify(balance, [&o](time_lock_balance_object& b) {
			b.amount -= o.amount;
		});
		d.adjust_balance(o.recipient, o.amount);
		d.remove(withdrawal);
	}
} }  // Namespace graphene::chain