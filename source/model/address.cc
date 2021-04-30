// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "address.h"

namespace boost::property_tree 
{
  // limits the decimal places in json output for doubles
  template <typename Ch, typename Traits>
  struct customize_stream<Ch, Traits, double, void> {
    static void insert(std::basic_ostream<Ch, Traits>& s, const double& e) 
    { s.precision(8); s << e; }
    static void extract(std::basic_istream<Ch, Traits>& s, double& e) 
    { s >> e; if(!s.eof()) { s >> std::ws; } }
  };
}

namespace sentio::model 
{

building building::from_json(json_t const& b)
{
  return model::building{
    .id = b.get<int64_t>("id"),
    .coords = spacial::coordinates(
      b.get<double>("coords.latitude"),
      b.get<double>("coords.longitude")),
    .country = std::string(),
    .city = b.get<std::string>("city"),
    .zipcode = b.get_optional<std::string>("zipcode").value_or(""),
    .street = b.get<std::string>("street"),
    .number = b.get<std::string>("number")
  };
}

json_t building::to_json() const
{
  json_t output;
  output.add("id", id);
  output.add("coords.latitude", coords.latitude());
  output.add("coords.longitude", coords.longitude());
  output.add("city", city);
  output.add("street", street);
  output.add("number", number);
  output.add("zipcode", zipcode);
  return output;
}

}  // namespace sentio::model
