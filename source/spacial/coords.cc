// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "coords.h"

namespace sentio::spacial
{

coordinates::coordinates()
  : lat_(0)
  , lng_(0)
{
}

coordinates::coordinates(json_t const& json)
  : coordinates(from_json(json)) {}

coordinates::coordinates(double lat, double lng)
    : lat_(lat)
    , lng_(lng)
{
}

double coordinates::latitude() const 
{ return lat_; }

double coordinates::longitude() const 
{ return lng_; }

double& coordinates::latitude() 
{ return lat_; }

double& coordinates::longitude() 
{ return lng_; }

bool coordinates::empty() const 
{ return longitude() <= 0 && latitude() <= 0; }

bool coordinates::operator==(coordinates const& other) const
{ 
  return latitude() == other.latitude() && 
         longitude() == other.longitude();
}

bool coordinates::operator!=(coordinates const& other) const
{ return !(*this == other); }


coordinates coordinates::from_json(json_t const& j) 
{
  return coordinates(
    j.get<double>("latitude"),
    j.get<double>("longitude"));
}

}