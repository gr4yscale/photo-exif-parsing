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

using namespace std;
using namespace boost::filesystem;
using namespace boost::gregorian;
using namespace boost::local_time;
using namespace boost::posix_time;

struct Photo
{
    string identifier;
    string fileName;
    string timeStamp;
    
    double latitude;
    double longitude;
    double altitude;
    
    double time_taken;
    boost::uintmax_t fileSize;
};

vector<Photo> photos;

int parseImage(const char *fileName, EXIFInfo &result);
void addPhoto(const char *fileName, EXIFInfo &result);
void printExifInfo(const char *fileName, EXIFInfo &result);
bool sortPhotoByDate (Photo a, Photo b);
void findBursts();
void createBurstDirectoryAndMovePhoto(Photo &photo, unsigned long index, stringstream &baseDirSS);
string sanitizedTimestamp(string &timestamp);


int main(int argc, const char * argv[])
{
    int fileCount = 0;
    
    path p = path("/Volumes/1TB Ext SSD 1/[iphone pix] copy");
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
                }
            }
            *it++;
            fileCount++;
        }
        catch (filesystem_error &e)
        {
            cerr << e.what() << '\n';
        }
    }
    
    findBursts();
    
    printf("***** DONE! *****");
    
    return 0;
}


void findBursts() {
    
    // we can't properly calculate the difference between timestamps without sorting first!
    sort (photos.begin(), photos.end(), sortPhotoByDate);
    
    bool burstHasStarted = false;
    double numberOfPhotosInBurst = 0;
    
    stringstream burstDirectoryStringStream;
    vector<Photo> photosToMove;
    Photo previousPhoto = photos[0];
    
    for (vector<Photo>::iterator it=photos.begin(); it!=photos.end(); ++it) {
        
        Photo photo = *it;
        double time = photo.time_taken;
        double difference = photo.time_taken - previousPhoto.time_taken;
        
        // if the time difference between the last photo in the iteration and the current one is small (1.5) we know it was a burst.
        // hypothetically. there is still the issue of frames dropping when the phone gets slow, but this is unavoidable.
        // -999 is for images we can't read the timestamp from.
        
        if (difference < 1.5 && time != -999) {
            numberOfPhotosInBurst++;
            if (!burstHasStarted) burstHasStarted = true;
            photosToMove.push_back(previousPhoto);
            
        } else {
            // if the current photo in the interation didn't have a timestamp with a low difference, check to see if we
            // were in a burst sequence. if so, break out and moves photos from the batch we collect
            
            if (burstHasStarted) {
                burstHasStarted = false;
                photosToMove.push_back(previousPhoto);
                
                // because of my hyperactive photo taking activity, ensure this is a true burst and not me just tapping the capture button rapidly
                
                if (numberOfPhotosInBurst > 10) {
                    for(std::vector<int>::size_type i = 0; i != photosToMove.size(); i++) {
                        createBurstDirectoryAndMovePhoto(photosToMove[i], i, burstDirectoryStringStream);
                    }
                }
                
                numberOfPhotosInBurst = 0;
                burstDirectoryStringStream.str(string()); //clear the stringstream
                photosToMove.clear();
            }
        }
        previousPhoto = photo;
    }
}

void createBurstDirectoryAndMovePhoto(Photo &photo, unsigned long index, stringstream &burstDirectoryStringStream) {
    
    string basePath = "/Volumes/1TB Ext SSD 1/[iPhone bursts]/";
    string sanitizedTimeStamp = sanitizedTimestamp(photo.timeStamp);
    
    path from = path(photo.fileName);
    
    // if this is the first photo from the burst, create a directory for it
    if (index == 0) {
        burstDirectoryStringStream << basePath << sanitizedTimeStamp.c_str();
        path dir = path(burstDirectoryStringStream.str());
        printf("***creating dir: %s\n", dir.c_str());
        create_directories(dir);
    }
    
    stringstream toPathStringStream;
    toPathStringStream << burstDirectoryStringStream.str() << "/" << index << "_" << path(photo.fileName).filename().c_str();
    
    path to = path(toPathStringStream.str());
    
    rename(from, to);
    
    if (index == 0) {
        copy_file(to,from);
    }
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
    stringstream ss;
    ss.imbue(locale(ss.getloc(), input_facet));
    local_date_time dateTimePhotoTaken(not_a_date_time);
    ss.str(photo.timeStamp);
    ss >> dateTimePhotoTaken;
    
    if (dateTimePhotoTaken.to_string() == "not-a-date-time") {
        photo.time_taken = -999;
    } else {
        time_duration durationSince1970 = dateTimePhotoTaken.utc_time() - (ptime)date(1970,1,1);
        photo.time_taken = durationSince1970.total_seconds();
        if (!result.SubSecTimeOriginal.empty()) {
            photo.time_taken += (stod(result.SubSecTimeOriginal) / 1000.0);
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

bool sortPhotoByDate (Photo a, Photo b) {
    return a.time_taken < b.time_taken;
}

string sanitizedTimestamp(string &timestamp) {
    string from = ":";
    string to = "-";
    
    if(timestamp.empty()) return "";
    
    size_t start_pos = 0;
    while((start_pos = timestamp.find(from, start_pos)) != string::npos) {
        timestamp.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return string(timestamp);
}
