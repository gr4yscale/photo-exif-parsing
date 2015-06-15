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
void addPhoto(const char *fileName, EXIFInfo &result);
void printExifInfo(const char *fileName, EXIFInfo &result);
bool sortPhotoDate (Photo a, Photo b);
void findBursts();

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
                const char *fileName;
                fileName = item.c_str();
                int retVal = parseImage(fileName, result);
                if (!retVal) {
                    addPhoto(fileName, result);
//                    printExifInfo(fileName, result);
                }
            }
            *it++;
            fileCount++;
        }
        catch (filesystem_error &e)
        {
            std::cerr << e.what() << '\n';
        }
    }
    
    findBursts();
    
    return 0;
}

int parseImage(const char *fileName, EXIFInfo &result) {
    FILE *fp = fopen(fileName, "rb");
    if (!fp) {
        printf("Can't open file.\n");
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    unsigned long fsize = ftell(fp);
    rewind(fp);
    
    unsigned char *buf = new unsigned char[fsize];
    
    if (fread(buf, 1, fsize, fp) != fsize) {
        printf("Can't read file.\n");
        delete[] buf;
        return -2;
    }
    fclose(fp);
    
    int retval = result.parseFrom(buf, (int)fsize);
    delete[] buf;
    
    if (retval) {
        printf("Error parsing EXIF: code %d  |  %s\n", retval, fileName);
        return -3;
    }
    return retval;
}

void addPhoto(const char *fileName, EXIFInfo &result) {
    Photo photo;
    photo.fileName = fileName;
    photo.fileSize = file_size(path(fileName));;
    photo.timeStamp = result.DateTimeOriginal;
    
    local_time_input_facet *input_facet = new local_time_input_facet("%Y-%m-%d %H:%M:%S %ZP");
    std::stringstream ss;
    ss.imbue(std::locale(ss.getloc(), input_facet));
    local_date_time dateTimePhotoTaken(not_a_date_time);
    ss.str(photo.timeStamp);
    ss >> dateTimePhotoTaken;
    
    if (dateTimePhotoTaken.to_string()  == "not-a-date-time") {
        photo.time_taken = -999;
    } else {
        time_duration durationSince1970 = dateTimePhotoTaken.utc_time() - (ptime)date(1970,1,1);
        photo.time_taken = durationSince1970.total_seconds();
        if (!result.SubSecTimeOriginal.empty()) {
            photo.time_taken += (std::stod(result.SubSecTimeOriginal) / 1000.0);
        }
    }

    photos.push_back(photo);
}

void printExifInfo(const char *fileName, EXIFInfo &result) {

    printf("Software          : %s\n", result.Software.c_str());
    printf("Image width       : %d\n", result.ImageWidth);
    printf("Image height      : %d\n", result.ImageHeight);
    printf("Image description : %s\n", result.ImageDescription.c_str());
    printf("Image orientation : %d\n", result.Orientation);
    printf("Original date/time: %s\n", result.DateTimeOriginal.c_str());
    printf("Subsecond time    : %s\n", result.SubSecTimeOriginal.c_str());
    printf("Exposure time     : 1/%d s\n", (unsigned) (1.0/result.ExposureTime));
    printf("F-stop            : f/%.1f\n", result.FNumber);
    printf("ISO speed         : %d\n", result.ISOSpeedRatings);
    printf("Subject distance  : %f m\n", result.SubjectDistance);
    printf("Lens focal length : %f mm\n", result.FocalLength);
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

}
