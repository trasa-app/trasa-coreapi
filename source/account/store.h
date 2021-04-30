// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#pragma once

#include <string>
#include <vector>
#include <optional>

#include "model.h"

namespace sentio::account
{
/**
 * This class provides persistence for account objects in dynamodb.
 */
class repository {
public:
  /**
   * Initialize the repository to the actual table name in AWS.
   * This value is based on the running environment and is retreived
   * from the configuration.
   */
  repository(std::string table);

  /**
   * Defaulted in the source file.
   * Needs to be declared for the pimpl pattern to work with unique_ptr.
   */
  repository(repository&&);
  ~repository();

public:
  /**
   * Attempts to retreive the account object identified by the given uid.
   * If no account with the given id is found, then an empty optional is 
   * returned.
   */
  std::optional<account> retrieve(std::string uid) const;

  /**
   * Inserts or updates an account in persistent storage.
   * 
   * If an account with the same ID is already stored in the database,
   * then this object will be merged with the existing one and saved
   * in dynamodb, otherwise a new one will be created.
   * 
   * Upon successful execution, it will return the complete merged 
   * and stored account object.
   */
  account upsert(account account) const;

private:
  class impl;
  std::unique_ptr<impl> impl_;
};

}
