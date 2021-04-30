# Map Data Import

Code in this folder is responsible for ingesting OpenStreetMaps data into internal representations for geocoding and routing.

## Data Source

- At the moment we are downloading data from http://download.geofabrik.de/europe/poland.html
- Map data is downloaded from individual osm.pbf files. That is what the geocoder and the router uses as their data source.
- The bounding polygons are downloaded from precomputed .poly files available at URLs with the following pattern:
  - http://download.geofabrik.de/europe/poland/podlaskie.poly
  - http://download.geofabrik.de/europe/poland/pomorskie.poly
  - etc.

## Data Ingestion

- We are using osmtools (`osmconvert` and `osmfilter`) to convert osm.pbf files into csv files with a list of all known addresses and their GPS coordinates for each voivodeship separately. 

- The conversion process could be replicated by calling this sequence of commands:

```
$ wget https://download.geofabrik.de/europe/poland/pomorskie-latest.osm.pbf
$ osmconvert pomorskie-latest.osm.pbf --all-to-nodes -o=pomorskie-latest.o5m
$ osmfilter pomorskie-latest.o5m --keep="addr:housenumber=*" | osmconvert - --csv="@id @lon @lat addr:country addr:city addr:postcode addr:street addr:housenumber" -o=world.csv  --csv-separator=";"
```

- For convenience and ease of deployment, the source codes of `osmconvert` and `osmfilter` are embedded in the main server executable as is. Otherwise, they  could be installed with:
````
apt install osmctools
````
