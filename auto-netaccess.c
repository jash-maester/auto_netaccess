#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define MAX_RESPONSE_SIZE 1024 * 1024  // 1MB buffer
#define MAX_COOKIE_SIZE 4096
#define BASE_URL "https://netaccess.iitm.ac.in"

// Structure to hold response data
struct response_data {
    char *data;
    size_t size;
};

// Structure to hold session data
struct session_data {
    char cookies[MAX_COOKIE_SIZE];
    CURL *curl;
};

// Callback function to write response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, struct response_data *response) {
    size_t total_size = size * nmemb;
    size_t new_size = response->size + total_size;
    
    if (new_size >= MAX_RESPONSE_SIZE) {
        printf("Response too large, truncating\n");
        return 0;
    }
    
    response->data = realloc(response->data, new_size + 1);
    if (!response->data) {
        printf("Memory allocation failed\n");
        return 0;
    }
    
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size = new_size;
    response->data[response->size] = '\0';
    
    return total_size;
}

// Initialize session
int init_session(struct session_data *session) {
    session->curl = curl_easy_init();
    if (!session->curl) {
        printf("Failed to initialize CURL\n");
        return 0;
    }
    
    // Set common options
    curl_easy_setopt(session->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(session->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(session->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(session->curl, CURLOPT_USERAGENT, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(session->curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(session->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(session->curl, CURLOPT_COOKIEFILE, ""); // Enable cookie engine
    
    memset(session->cookies, 0, sizeof(session->cookies));
    
    return 1;
}

// Perform login
int perform_login(struct session_data *session, const char *username, const char *password) {
    struct response_data response = {0};
    char post_data[512];
    char login_url[256];
    CURLcode res;
    
    printf("Attempting to login...\n");
    
    // First, GET the login page to establish session
    snprintf(login_url, sizeof(login_url), "%s/", BASE_URL);
    curl_easy_setopt(session->curl, CURLOPT_URL, login_url);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(session->curl, CURLOPT_HTTPGET, 1L);
    
    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Failed to access login page: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }
    
    // Check response
    long response_code;
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        printf("Login page access failed with HTTP %ld\n", response_code);
        free(response.data);
        return 0;
    }
    
    free(response.data);
    response.data = NULL;
    response.size = 0;
    
    // Now perform POST login
    snprintf(login_url, sizeof(login_url), "%s/account/login", BASE_URL);
    snprintf(post_data, sizeof(post_data), 
        "userLogin=%s&userPassword=%s&submit=Log%%20in", 
        username, password);
    
    curl_easy_setopt(session->curl, CURLOPT_URL, login_url);
    curl_easy_setopt(session->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);
    
    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Login request failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }
    
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        printf("Login failed with HTTP %ld\n", response_code);
        free(response.data);
        return 0;
    }
    
    // Check if login was successful
    int login_success = 0;
    if (response.data && 
        (strstr(response.data, "logout") || strstr(response.data, "Logout") ||
         strstr(response.data, "Authorized machines") || strstr(response.data, "authorized machines"))) {
        login_success = 1;
    }
    
    free(response.data);
    
    if (login_success) {
        printf("Login successful\n");
        return 1;
    } else {
        printf("Login failed - invalid credentials or other error\n");
        return 0;
    }
}

// Approve machine for internet access
int approve_machine(struct session_data *session, int duration) {
    struct response_data response = {0};
    char approve_url[256];
    char post_data[128];
    CURLcode res;
    
    printf("Attempting to approve machine for %s...\n", 
           duration == 1 ? "1 hour" : "1 day");
    
    // First GET the approve page
    snprintf(approve_url, sizeof(approve_url), "%s/account/approve", BASE_URL);
    curl_easy_setopt(session->curl, CURLOPT_URL, approve_url);
    curl_easy_setopt(session->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);
    
    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Failed to access approve page: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }
    
    long response_code;
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        printf("Approve page access failed with HTTP %ld\n", response_code);
        free(response.data);
        return 0;
    }
    
    free(response.data);
    response.data = NULL;
    response.size = 0;
    
    // Now POST the approval
    snprintf(post_data, sizeof(post_data), 
        "duration=%d&approveBtn=Authorize", duration);
    
    curl_easy_setopt(session->curl, CURLOPT_URL, approve_url);
    curl_easy_setopt(session->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(session->curl, CURLOPT_WRITEDATA, &response);
    
    res = curl_easy_perform(session->curl);
    if (res != CURLE_OK) {
        printf("Machine approval request failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }
    
    curl_easy_getinfo(session->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        printf("Machine approval failed with HTTP %ld\n", response_code);
        free(response.data);
        return 0;
    }
    
    // Check for success (most approval pages will redirect or show success message)
    printf("Machine approval request submitted successfully\n");
    
    free(response.data);
    return 1;
}

// Cleanup session
void cleanup_session(struct session_data *session) {
    if (session->curl) {
        curl_easy_cleanup(session->curl);
        session->curl = NULL;
    }
}

int main(int argc, char *argv[]) {
    struct session_data session;
    const char *username = "USERNAME";
    const char *password = "PASSWORD";
    int duration = 2; // 1 day by default
    
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
    
    printf("NetAccess Authentication Tool\n");
    printf("Username: %s\n", username);
    printf("Duration: %s\n", duration == 1 ? "1 hour" : "1 day");
    printf("========================================\n");
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Initialize session
    if (!init_session(&session)) {
        curl_global_cleanup();
        return 1;
    }
    
    // Perform authentication workflow
    int success = 0;
    if (perform_login(&session, username, password)) {
        if (approve_machine(&session, duration)) {
            printf("========================================\n");
            printf("Automation completed successfully!\n");
            success = 1;
        }
    }
    
    if (!success) {
        printf("========================================\n");
        printf("Automation failed - check network connection and credentials\n");
    }
    
    // Cleanup
    cleanup_session(&session);
    curl_global_cleanup();
    
    return success ? 0 : 1;
}
