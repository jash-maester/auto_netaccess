#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_RESPONSE_SIZE (1024 * 1024) // 1MB buffer
#define MAX_COOKIE_SIZE 4096
#define BASE_URL "https://netaccess.iitm.ac.in"


// Structure to hold response data
struct ResponseData {
    char *data;
    size_t size;
};

// Structure to hold session data
struct SessionData {
    char cookies[MAX_COOKIE_SIZE];
    CURL *curl;
};

// Callback function to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct ResponseData *response) {
    size_t totalSize = size * nmemb;
    size_t newSize = response->size + totalSize;

    if (newSize > MAX_RESPONSE_SIZE) {
        printf("Response too large, truncating\n");
        return 0;
    }

    response->data = realloc(response->data, newSize + 1);
    if (!response->data) {
        printf("Memory allocation failed\n");
        return 0;
    }

    memcpy(&response->data[response->size], contents, totalSize);
    response->size = newSize;
    response->data[response->size] = '\0';

    return totalSize;
}

// Initialize session
int InitSession(struct SessionData *session) {
    session->curl = curl_easy_init();
    if (!session->curl) {
        printf("Failed to initialize CURL\n");
        return 0;
    }

    // Set common options
    curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(session->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(session->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(session->curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(session->curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(session->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(session->curl, CURLOPT_COOKIEFILE, ""); // Enable cookie engine

    memset(session->cookies, 0, sizeof(session->cookies));
    return 1;
}

// Extract CSRF token or other hidden fields from HTML
char* ExtractValue(const char *html, const char *name) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "name=\"%s\" value=\"", name);
    
    char *start = strstr(html, pattern);
    if (!start) return NULL;
    
    start += strlen(pattern);
    char *end = strchr(start, '"');
    if (!end) return NULL;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    if (value) {
        memcpy(value, start, len);
        value[len] = '\0';
    }
    return value;
}

// Perform login
int PerformLogin(struct SessionData *session, const char *username, const char *password) {
    struct ResponseData response = {0};
    char postData[512];
    char loginUrl[256];
    CURLcode res;

    printf("Attempting to login...\n");

    // First, GET the login page to establish session
    snprintf(loginUrl, sizeof(loginUrl), "%s/login", BASE_URL);
    curl_easy_setopt(session->curl, CURLOPT_URL, loginUrl);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(session->curl, CURLOPT_HTTPGET, 1L);

    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Failed to access login page: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    // Check response
    long responseCode;
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &responseCode);
    if (responseCode != 200) {
        printf("Login page access failed with HTTP %ld\n", responseCode);
        free(response.data);
        return 0;
    }

    free(response.data);
    response.data = NULL;
    response.size = 0;

    // Now perform POST login (form action is just POST to same URL based on the HTML)
    snprintf(postData, sizeof(postData), "username=%s&password=%s", username, password);
    
    curl_easy_setopt(session->curl, CURLOPT_URL, loginUrl);
    curl_easy_setopt(session->curl, CURLOPT_POSTFIELDS, postData);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Login request failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &responseCode);
    if (responseCode != 200) {
        printf("Login failed with HTTP %ld\n", responseCode);
        free(response.data);
        return 0;
    }

    // Check if login was successful by looking for dashboard elements
    int loginSuccess = 0;
    if (response.data && (strstr(response.data, "Dashboard") || 
                         strstr(response.data, "Approve a Device") ||
                         strstr(response.data, "User Profile"))) {
        loginSuccess = 1;
    }

    free(response.data);

    if (loginSuccess) {
        printf("Login successful!\n");
        return 1;
    } else {
        printf("Login failed - invalid credentials or other error\n");
        return 0;
    }
}

// Approve machine for internet access
int ApproveMachine(struct SessionData *session, int duration) {
    struct ResponseData response = {0};
    char approveUrl[256];
    char postData[256];
    CURLcode res;

    printf("Attempting to approve machine for %s...\n", (duration == 1) ? "1 hour" : "1 day");

    // First GET the approve page
    snprintf(approveUrl, sizeof(approveUrl), "%s/approve", BASE_URL);
    curl_easy_setopt(session->curl, CURLOPT_URL, approveUrl);
    curl_easy_setopt(session->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Failed to access approve page: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    long responseCode;
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &responseCode);
    if (responseCode != 200) {
        printf("Approve page access failed with HTTP %ld\n", responseCode);
        free(response.data);
        return 0;
    }

    // Extract hidden field values if they exist
    char *ip = ExtractValue(response.data, "ip");
    char *username = ExtractValue(response.data, "username");
    char *building = ExtractValue(response.data, "building");

    free(response.data);
    response.data = NULL;
    response.size = 0;

    // Build POST data for approval
    // Based on the HTML, the form has: duration, ip (hidden), username (hidden), building (hidden)
    if (ip && username && building) {
        snprintf(postData, sizeof(postData), 
                "duration=%d&ip=%s&username=%s&building=%s", 
                duration, ip, username, building);
    } else {
        // Fallback if we can't extract hidden values
        snprintf(postData, sizeof(postData), "duration=%d", duration);
    }

    // printf("Submitting approval with data: %s\n", postData);

    // POST the approval
    curl_easy_setopt(session->curl, CURLOPT_URL, approveUrl);
    curl_easy_setopt(session->curl, CURLOPT_POSTFIELDS, postData);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(session->curl);

    // Cleanup extracted values
    free(ip);
    free(username);  
    free(building);

    if (res != CURLE_OK) {
        printf("Machine approval request failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &responseCode);
    if (responseCode != 200 && responseCode != 302) { // 302 redirect is also acceptable
        printf("Machine approval failed with HTTP %ld\n", responseCode);
        free(response.data);
        return 0;
    }

    // Check for success indicators in response
    int success = 0;
    if (response.data) {
        if (strstr(response.data, "success") || 
            strstr(response.data, "approved") ||
            strstr(response.data, "authorized") ||
            responseCode == 302) { // Redirect usually means success
            success = 1;
        }
    }

    printf("Machine approval request submitted successfully!\n");
    free(response.data);
    return success;
}

// Cleanup session
void CleanupSession(struct SessionData *session) {
    if (session->curl) {
        curl_easy_cleanup(session->curl);
        session->curl = NULL;
    }
}

int main(int argc, char *argv[]) {
    struct SessionData session;
    const char *username = "USERNAME"; // Replace with your username
    const char *password = "PASSWORD"; // Replace with your password
    int duration = 2; // 1 day by default (1=1hour, 2=1day)

    // Allow command line arguments for credentials
    if (argc >= 3) {
        username = argv[1];
        password = argv[2];
    }
    if (argc >= 4) {
        duration = atoi(argv[3]);
        if (duration != 1 && duration != 2) {
            printf("Duration must be 1 (1 hour) or 2 (1 day)\n");
            return 1;
        }
    }

    printf("=== NetAccess Authentication Tool ===\n");
    printf("Username: %s\n", username);
    printf("Duration: %s\n", (duration == 1) ? "1 hour" : "1 day");
    printf("=====================================\n");

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize session
    if (!InitSession(&session)) {
        curl_global_cleanup();
        return 1;
    }

    // Perform authentication workflow
    int success = 0;
    if (PerformLogin(&session, username, password)) {
        if (ApproveMachine(&session, duration)) {
            printf("\n✅ Automation completed successfully!\n");
            success = 1;
        }
    }

    if (!success) {
        printf("\n❌ Automation failed - check network connection and credentials\n");
    }

    // Cleanup
    CleanupSession(&session);
    curl_global_cleanup();

    return success ? 0 : 1;
}

