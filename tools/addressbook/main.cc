// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include <string>
#include <locale>
#include <codecvt>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include <sqlite3.h>
#include <import/region_reader.h>

#define ensure_ok(expr) do {                  \
  if(expr != SQLITE_OK) {                     \
    throw std::runtime_error("sqlite error"); \
  }                                           \
} while(0)                                    \

#define ensure_ok_db(expr, db) do {               \
  if(expr != SQLITE_OK) {                         \
    throw std::runtime_error(sqlite3_errmsg(db)); \
  }                                               \
} while(0)                                        \


void normalize_pl(std::wstring& original)
{
  // not using full-blown unicoode normalization from ICU or
  // boost locale, because for now we practically only care about
  // polish accents.

  for (auto& c: original) {
    c = std::towlower(c);
    switch (c) {
      // towlower() doesn't treat
      // polish accensts such as Ł 
      // as uppercase of ł, 
      // or ł as the same family as l
      
      // uppercase accents
      case L'Ą': c = L'a'; break;
      case L'Ę': c = L'e'; break;
      case L'Ć': c = L'c'; break;
      case L'Ł': c = L'l'; break;
      case L'Ń': c = L'n'; break;
      case L'Ó': c = L'o'; break;
      case L'Ś': c = L's'; break;
      case L'Ż': c = L'z'; break;
      case L'Ź': c = L'z'; break;

      // lowercase
      case L'ą': c = L'a'; break;
      case L'ę': c = L'a'; break;
      case L'ć': c = L'c'; break;
      case L'ł': c = L'l'; break;
      case L'ń': c = L'n'; break;
      case L'ó': c = L'o'; break;
      case L'ś': c = L's'; break;
      case L'ż': c = L'z'; break;
      case L'ź': c = L'z'; break;
      default: break;
    }
  }
}

std::string generate_alternatives(std::string const& original)
{
  if (!original.empty()) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> s_conv;
    std::wstring wstr(s_conv.from_bytes(original));
    normalize_pl(wstr);
    return s_conv.to_bytes(wstr);
  } 
  return original;
}

void insert_building(sentio::model::building const& b, sqlite3* db)
{
  const char* insert_sqlite_building_sql = 
    "INSERT INTO building(id, longitude, latitude, country, "
    "city, zipcode, street, number, alt_street, alt_city) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  
  sqlite3_stmt* insert_statement = nullptr;
  ensure_ok_db(sqlite3_prepare_v2(db, 
    insert_sqlite_building_sql, -1, 
    &insert_statement, nullptr), db);

  std::string alt_city(generate_alternatives(b.city));
  std::string alt_street(generate_alternatives(b.street));

  ensure_ok_db(sqlite3_bind_int64(insert_statement, 1,  
    b.id), db);

  ensure_ok_db(sqlite3_bind_double(insert_statement, 2,  
    b.coords.longitude()), db);

  ensure_ok_db(sqlite3_bind_double(insert_statement, 3,  
    b.coords.latitude()), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 4,  
    b.country.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 5,  
    b.city.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 6,  
    b.zipcode.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 7,  
    b.street.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 8,  
    b.number.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 9,  
    alt_street.c_str(), -1, SQLITE_TRANSIENT), db);

  ensure_ok_db(sqlite3_bind_text(insert_statement, 10, 
    alt_city.c_str(), -1, SQLITE_TRANSIENT), db);


  if (sqlite3_step(insert_statement) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(db));
  }

  ensure_ok_db(sqlite3_finalize(insert_statement), db);
}

void create_database(sqlite3* db)
{
  const char* create_sqlite_schema_sql = 
    "CREATE VIRTUAL TABLE building USING fts5("
    "id UNINDEXED, longitude UNINDEXED, latitude UNINDEXED, "
    "country, city, zipcode, street, number, alt_street, alt_city, "
    "tokenize = \"unicode61 remove_diacritics 2\");";

  ensure_ok_db(sqlite3_exec(
    db, create_sqlite_schema_sql, 
    nullptr, nullptr, nullptr), db);
}

void optimize_database(sqlite3* db)
{
  const char* optimize_sqlite_schema_sql = 
    "INSERT INTO building(building) VALUES('optimize');";

  ensure_ok_db(sqlite3_exec(
    db, optimize_sqlite_schema_sql, 
    nullptr, nullptr, nullptr), db);
}

void optimize_inserts(sqlite3* db)
{
  ensure_ok_db(sqlite3_exec(db, 
    "PRAGMA synchronous=OFF", 
    nullptr, nullptr, nullptr), db);

  ensure_ok_db(sqlite3_exec(db, 
    "PRAGMA count_changes=OFF", 
    nullptr, nullptr, nullptr), db);

  ensure_ok_db(sqlite3_exec(db, 
    "PRAGMA journal_mode=MEMORY", 
    nullptr, nullptr, nullptr), db);

  ensure_ok_db(sqlite3_exec(db, 
    "PRAGMA temp_store=MEMORY", 
    nullptr, nullptr, nullptr), db);
}

int main(int argc, const char** argv)
{
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] 
              << " <input-csv-file> <output-sqlite-file>" 
              << std::endl;
    return 1;
  }

  std::string inputfile(argv[1]);
  std::string outputfile(argv[2]);

  sentio::import::building_record_iterator sentinel;
  sentio::import::building_record_iterator record_it(inputfile);

  if (std::filesystem::exists(outputfile)) {
    std::filesystem::remove(outputfile);
  }

  sqlite3* dbptr = nullptr;
  ensure_ok(sqlite3_open(outputfile.c_str(), &dbptr));
  
  create_database(dbptr);
  optimize_inserts(dbptr);

  size_t counter = 0;
  std::cout << "indexing building: " << std::endl;
  for (; record_it != sentinel; ++record_it, ++counter) {
    insert_building(*record_it, dbptr);
    std::cout  << "\033[A\33[2K\rindexing building: "
               << counter << std::endl;
  }

  // rebuild and compress indecies after all 
  // data has been inserted and tokenized.
  optimize_database(dbptr);
  
  // release and flush database to disk
  ensure_ok_db(sqlite3_close(dbptr), dbptr);

  std::cout << std::endl << "buildings: " << counter << std::endl;;
  std::cout << "sqlite version: " << sqlite3_libversion() << std::endl;
  return 0;
}