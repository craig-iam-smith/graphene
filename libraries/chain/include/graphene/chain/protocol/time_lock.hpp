namespace graphene { namespace chain {

// Operation to create a new time-lock balance with a specified asset type,
// initail deposit, and review period duration
    
struct time_lock_create_operation : public base_operation {

  struct fee_parameters_type {
    // To set the fee to 1 CORE:
    share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
  };

  asset fee;
  account_id_type owner;
  // Amount of asset to be deposited into the time-lock balance immediately
  // Amount may be zero, but the asset type must be set correctly or the
  // balance will store the wrong type of currency
  asset initial_deposit;
  // Duration to hold withdrawals in review before executing them
  int64_t review_period_seconds;
  // We could move these method implementations to the source file, ut
  // they're trivial so we'll inline them here
  
  account_id_type fee_payer() const {  return owner; }
  // This method performs internal consistency checks on the operation,
  // and throws an exception of the operation is invalid.
  void validate() const {
    FC_ASSERT(fee.amount > 0, "Fee must be positive.");
    FC_ASSERT(initial_deposit.amount >= 0, "Initial deposit must be non-negative.");
    FC_ASSERT(review_period_seconds > 0, "Review period must be positive.");
  }
  // This method return the amount of the fee for the operation.
  share_type calculate_fee(const fee_parameters_type& k) const {
    return k.fee;
  }
};

    // operation to deposit funds into a time-lock balance
struct time_lock_deposit_operation : public base_operation {
  struct fee_parameters_type {
    share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
  };

  asset fee;
  account_id_type owner;
  // ID of the balance to deposit funds to
  // We'll address where time_lock_balance_id_type was defined later
  time_lock_balance_id_type balance;
  // Amount to deposit.  We could use share_type here instead of asset, as
  // the asset_id_type can be inferred fro the balance; however, we
  // include it anyways to make the operation more solf-documenting.
  asset deposit;

  account_id_type fee_payer() const { return owner; }
  void validate() const {
    FC_ASSERT(fee.amount > 0);
    FC_ASSERT(deposit.amount > 0, "Deposit must be positive.");
  }
  share_type calculate_fee(const fee_parameters_type& k) const {
    return k.fee;
  }
};
    // Operation to initiate a withdrawal from a time-lock balance
struct time_lock_withdraw_operation : public base_operation {
  struct fee_parameters_type {
    share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
  };
  asset fee;
  account_id_type owner;
  // ID of the balance to withdraw funds from
  time_lock_balance_id_type balance;
  // Amount to withdraw
  asset withdrawal;
  // Account to withdraw
  account_id_type recipient;

  account_id_type fee_payer() const { return owner; }
  void validate () const {
    FC_ASSERT(fee.amount > 0);
    FC_ASSERT(withdrawal.amount > 0, "Withdrawal must be positive.");
  }
  share_type calculate_fee(const fee_parameters_type& k) const {
    return k.fee;
  }
};

    // Operation to abort a pending withdrawal from a time-lock balance
struct time_lock_abort_withdrawal_operation : public base_operation {
  struct fee_parameters_type {
	// We can set a zero fee here, as this operation cannot be spammed
	// without paying many fees for time_lock_withdraw_operations
    share_type fee = 0;
  };

  asset fee;
  account_id_type owner;
  // ID of the withdrawal to abort
  time_lock_withdrawal_id_type withdrawal;

  account_id_type fee_payer() const { return owner; }
  void validate() const {
	// Note that we do permit a zero fee here
	   FC_ASSERT(fee.amount >= 0);
  }
  share_type calculate_fee (const fee_parameters_type& k) const {
	   return k.fee;
  }
};

    // Operation to complete a pending withdrawal from a time-lock balance.
    // This could be done automatically by the chain, without requiring a
    // transaction, but that's more complicated and best practice is to
    // have all movement of money be triggered by an operation.
struct time_lock_complete_withdrawal_operation : public base_operation {
  struct fee_parameters_type {
  	share_type fee = 0;
  };

  asset fee;
  // This may be either the owner or the recipient
  account_id_type acting_account;
  // The account to receive the withdrawn funds
  // Must match withdrawal->recipient
  // Included to make the operation self-documenting
  account_id_type recipient;
  // The amount to withdraw. Must match withdrawal->withdrawal
  // Included to make the operation self-documenting
  asset amount;
  time_lock_withdrawal_id_type withdrawal;

  account_id_type fee_payer() const { return acting_account; }
  void validate() const {
	 FC_ASSERT(fee.amount >= 0);
	 FC_ASSERT(amount.amount > 0, "Withdrawal must be positive.");
  }
  share_type calculate_fee(const fee_parameters_type& k) const {
	  return k.fee;
  }
};
	
} } // namespace graphene::chain

// We must also register these structs with FC's serialization infrastructure
// by identifying them and listing out their fields:
FC_REFLECT(graphene::chain::time_lock_create_operation::fee_parameters_type, (fee));
FC_REFLECT(graphene::chain::time_lock_create_operation,
	   (fee)(owner)(initial_deposit)(review_period_seconds));

FC_REFLECT(graphene::chain::time_lock_deposit_operation::fee_parameters_type, (fee));
FC_REFLECT(graphene::chain::time_lock_deposit_operation,
	   (fee)(owner)(balance)(deposit));

FC_REFLECT(graphene::chain::time_lock_withdraw_operation::fee_parameters_type, (fee));
FC_REFLECT(graphene::chain::time_lock_withdraw_operation,
	   (fee)(owner)(balance)(withdrawal)(recipient));

FC_REFLECT(graphene::chain::time_lock_abort_withdrawal_operation::fee_parameters_type, (fee));
FC_REFLECT(graphene::chain::time_lock_abort_withdrawal_operation,
	   (fee)(owner)(withdrawal));

FC_REFLECT(graphene::chain::time_lock_complete_withdrawal_operation::fee_parameters_type, (fee));
FC_REFLECT(graphene::chain::time_lock_complete_withdrawal_operation,
	   (fee)(acting_account)(recipient)(amount)(withdrawal));

