#ifndef PFW_CORE_FOUNDATION_HELPER_H
#define PFW_CORE_FOUNDATION_HELPER_H

#if defined(__APPLE__)

#include <CoreServices/CoreServices.h>

std::string getStringValue(CFStringRef cfRef, CFStringEncoding encoding = kCFStringEncodingUTF8)
    {
        const char * s;
        char       * buf;
        size_t       length;
        std::string  str;
        
        if( cfRef == NULL )
        {
            return "";
        }

            length = static_cast< size_t >( CFStringGetMaximumSizeForEncoding( CFStringGetLength( cfRef ), encoding ) );
            buf    = new char [ length + 1 ];
            
            memset( buf, 0, length + 1 );
            CFStringGetCString( cfRef, buf, static_cast< CFIndex >( length + 1 ), encoding );
            
            str = ( buf == NULL ) ? "" : buf;
            
            delete [] buf;
        
        return str;
    }

std::string getErrorInfo(CFErrorRef cferror) {
        if( cferror == NULL )
        {
            return "no error";
        }
        
        CFStringRef cfStrDomain = CFErrorGetDomain( cferror );
        CFStringRef cfStrDesc = CFErrorCopyDescription(cferror);
        
        long n = CFErrorGetCode( cferror );

        std::string res = "Domain: " + getStringValue(cfStrDomain) + " ErrorCode: " + std::to_string(n) + " Descritpion: " + getStringValue(cfStrDesc);
        
        return res;
}

std::string getSystemFileName(const fs::path &path)
{
    CFStringRef cfPath = CFStringCreateWithCString(NULL, path.string().c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(
        0, cfPath, kCFURLPOSIXPathStyle, false);
    if (!url) {
        return "url not valid";
    }

    CFErrorRef error;
    CFStringRef value;
    if (!CFURLCopyResourcePropertyForKey(url, kCFURLNameKey, &value, &error)) {
        return getErrorInfo(error);
    }
    return getStringValue(value);
}

#endif
