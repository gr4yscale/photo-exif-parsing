//
//  main.cpp
//  photo-exif-parsing
//
//  Created by Tyler Powers on 6/11/15.
//  Copyright (c) 2015 Tyler Powers. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <fstream>

#include "exif.h"
#include "json.h"

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;
using namespace boost::gregorian;
using namespace boost::local_time;
using namespace boost::posix_time;

struct Photo
{
    std::string identifier;
    std::string fileName;
    
    double latitude;
    double longitude;
    double altitude;
    
    double time_taken;
    boost::uintmax_t fileSize;
};

std::vector<Photo> photos;

int parseImage(const char *fileName, EXIFInfo &result);
void writeJSON();
void addPhoto(const char *fileName, EXIFInfo &result);
void printExifInfo(EXIFInfo &result);

int main(int argc, const char * argv[])
{
    int fileCount = 0;
    
    path p = path("/Volumes/1TB Ext SSD 1/[iphone pix]");
    directory_iterator it{p};
    
    while (it != directory_iterator{}) {
        try
        {
            path item = *it;
            if (item.extension() == ".JPG") {
                EXIFInfo result;
                int retVal = parseImage(item.c_str(), result);
                if (!retVal) {
                    printExifInfo(result);
                }
            }
        }
        catch (filesystem_error &e)
        {
            std::cerr << e.what() << '\n';
        }
        *it++;
        fileCount++;
    }
    
//    writeJSON();
    return 0;
}

int parseImage(const char *fileName, EXIFInfo &result) {
    FILE *fp = fopen(fileName, "rb");
    if (!fp)
    {
        printf("Can't open file.\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    unsigned long fsize = ftell(fp);
    rewind(fp);
    unsigned char *buf = new unsigned char[fsize];
    if (fread(buf, 1, fsize, fp) != fsize)
    {
        printf("Can't read file.\n");
        delete[] buf;
        return -2;
    }
    fclose(fp);
    
    int retval = result.parseFrom(buf, (int)fsize);
    delete[] buf;
    if (retval)
    {
        printf("Error parsing EXIF: code %d\n", retval);
        return -3;
    }
    return retval;
}

void writeJSON() {
    Json::Value root;
    Json::Value featureCollection;
    
    featureCollection["type"] = "FeatureCollection";
    
    Json::Value features;
    
    std::sort(photos.begin(), photos.end(), [ ](const Photo& a, const Photo& b)
    {
        return a.time_taken < b.time_taken;
    });
    
    for (std::vector<Photo>::iterator it=photos.begin(); it!=photos.end(); ++it)
    {
        Photo photo = *it;
        Json::Value geometry;
        Json::Value properties;
        Json::Value feature;
        Json::Value coordinates;
        
        coordinates.append(photo.longitude);
        coordinates.append(photo.latitude);
        
        geometry["coordinates"] = coordinates;
        geometry["type"] = "Point";
        
        feature["geometry"] = geometry;
        feature["type"] = "Feature";
        feature["properties"] = properties;
        features.append(feature);
    }
    
    featureCollection["features"] = features;
    
    Json::StyledWriter styledWriter;
    std::ofstream myfile;
    myfile.open ("/Users/gr4yscale/code/photo-exif-parsing/geojson.json");
    myfile << styledWriter.write(featureCollection);
    myfile.close();
}

void addPhoto(const char *fileName, EXIFInfo &result) {
    boost::uintmax_t filesize = file_size(path(fileName));
    std::string timestamp = result.DateTimeOriginal;
    
    local_time_input_facet *input_facet = new local_time_input_facet("%Y-%m-%d %H:%M:%S %ZP");
    std::stringstream ss;
    ss.imbue(std::locale(ss.getloc(), input_facet));
    local_date_time dateTimePhotoTaken(not_a_date_time);
    ss.str(timestamp);
    ss >> dateTimePhotoTaken;
    
    if (dateTimePhotoTaken.to_string()  == "not-a-date-time") return;
    
    time_duration durationSince1970 = dateTimePhotoTaken.utc_time() - (ptime)date(1970,1,1);
    
    Photo photo;
    photo.fileName = fileName;
    photo.latitude = result.GeoLocation.Latitude;
    photo.longitude = result.GeoLocation.Longitude;
    photo.altitude = result.GeoLocation.Altitude;
    photo.fileSize = filesize;
    photo.time_taken = durationSince1970.total_seconds();
    
    if (photo.latitude > 0 && photo.longitude > 0)
    {
        photos.push_back(photo);
    }
}

void printExifInfo(EXIFInfo &result) {
    printf("Camera make       : %s\n", result.Make.c_str());
    printf("Camera model      : %s\n", result.Model.c_str());
    printf("Software          : %s\n", result.Software.c_str());
    printf("Bits per sample   : %d\n", result.BitsPerSample);
    printf("Image width       : %d\n", result.ImageWidth);
    printf("Image height      : %d\n", result.ImageHeight);
    printf("Image description : %s\n", result.ImageDescription.c_str());
    printf("Image orientation : %d\n", result.Orientation);
    printf("Image copyright   : %s\n", result.Copyright.c_str());
    printf("Image date/time   : %s\n", result.DateTime.c_str());
    printf("Original date/time: %s\n", result.DateTimeOriginal.c_str());
    printf("Digitize date/time: %s\n", result.DateTimeDigitized.c_str());
    printf("Subsecond time    : %s\n", result.SubSecTimeOriginal.c_str());
    printf("Exposure time     : 1/%d s\n", (unsigned) (1.0/result.ExposureTime));
    printf("F-stop            : f/%.1f\n", result.FNumber);
    printf("ISO speed         : %d\n", result.ISOSpeedRatings);
    printf("Subject distance  : %f m\n", result.SubjectDistance);
    printf("Exposure bias     : %f EV\n", result.ExposureBiasValue);
    printf("Flash used?       : %d\n", result.Flash);
    printf("Metering mode     : %d\n", result.MeteringMode);
    printf("Lens focal length : %f mm\n", result.FocalLength);
    printf("35mm focal length : %u mm\n", result.FocalLengthIn35mm);
    printf("GPS Latitude      : %f deg (%f deg, %f min, %f sec %c)\n",
           result.GeoLocation.Latitude,
           result.GeoLocation.LatComponents.degrees,
           result.GeoLocation.LatComponents.minutes,
           result.GeoLocation.LatComponents.seconds,
           result.GeoLocation.LatComponents.direction);
    printf("GPS Longitude     : %f deg (%f deg, %f min, %f sec %c)\n",
           result.GeoLocation.Longitude,
           result.GeoLocation.LonComponents.degrees,
           result.GeoLocation.LonComponents.minutes,
           result.GeoLocation.LonComponents.seconds,
           result.GeoLocation.LonComponents.direction);
    printf("GPS Altitude      : %f m\n", result.GeoLocation.Altitude);
    
    printf("----------------------------------------------------------------\n");
}
