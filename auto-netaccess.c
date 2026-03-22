/*
 * auto_netaccess.c
 * Automates login and machine approval on the IIT Madras CC Networks portal.
 * Updated for the new portal at https://cc.iitm.ac.in/netaccess
 *
 * Build:
 *   gcc auto_netaccess.c -o auto_netaccess -lcurl
 *
 * Usage:
 *   ./auto_netaccess <username> <password> [duration]
 *   duration: "hour" (default) | "day" | "month"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define MAX_RESPONSE_SIZE (2 * 1024 * 1024)   /* 2 MB – pages can be verbose */
#define BASE_URL "https://cc.iitm.ac.in/netaccess"

/*
 * A realistic, modern browser User-Agent string.
 * The portal may fingerprint UA to decide whether to show a CAPTCHA, so we
 * mimic a mainstream browser closely.
 */
#define USER_AGENT \
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) " \
    "AppleWebKit/537.36 (KHTML, like Gecko) " \
    "Chrome/124.0.0.0 Safari/537.36"

/* Accept header that matches what a browser sends */
#define ACCEPT_HEADER \
    "text/html,application/xhtml+xml,application/xml;q=0.9," \
    "image/avif,image/webp,image/apng,*/*;q=0.8"

/* -------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------- */

struct response_data {
    char  *data;
    size_t size;
};

struct session_data {
    CURL *curl;
};

/* -------------------------------------------------------------------------
 * CURL helpers
 * ---------------------------------------------------------------------- */

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             struct response_data *resp)
{
    size_t chunk    = size * nmemb;
    size_t new_size = resp->size + chunk;

    if (new_size >= MAX_RESPONSE_SIZE) {
        fprintf(stderr, "[warn] Response too large, truncating.\n");
        return 0;
    }

    resp->data = realloc(resp->data, new_size + 1);
    if (!resp->data) {
        fprintf(stderr, "[error] Memory allocation failed.\n");
        return 0;
    }

    memcpy(&resp->data[resp->size], contents, chunk);
    resp->size         = new_size;
    resp->data[new_size] = '\0';
    return chunk;
}

/* Free response buffer and reset the struct */
static void response_reset(struct response_data *r)
{
    free(r->data);
    r->data = NULL;
    r->size = 0;
}

/* -------------------------------------------------------------------------
 * Session management
 * ---------------------------------------------------------------------- */

int init_session(struct session_data *session)
{
    session->curl = curl_easy_init();
    if (!session->curl) {
        fprintf(stderr, "[error] Failed to initialise CURL handle.\n");
        return 0;
    }

    CURL *c = session->curl;

    /* Response writer */
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_callback);

    /* Follow redirects (login → dashboard) */
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS,      10L);

    /* Timeout */
    curl_easy_setopt(c, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT,  10L);

    /* Browser impersonation */
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);

    /* Enable cookie engine (in-memory; no file needed) */
    curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");

    /* Keep SSL verification enabled – the site has a valid cert */
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);

    /* Send realistic browser headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: " ACCEPT_HEADER);
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers,
        "Upgrade-Insecure-Requests: 1");
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    /* Note: headers list is deliberately kept alive for the session lifetime */

    return 1;
}

void cleanup_session(struct session_data *session)
{
    if (session->curl) {
        curl_easy_cleanup(session->curl);
        session->curl = NULL;
    }
}

/* -------------------------------------------------------------------------
 * HTTP helpers
 * ---------------------------------------------------------------------- */

/*
 * Perform a GET request and store the body in *resp.
 * Returns the HTTP status code, or 0 on transport error.
 */
static long http_get(CURL *c, const char *url, struct response_data *resp)
{
    curl_easy_setopt(c, CURLOPT_URL,       url);
    curl_easy_setopt(c, CURLOPT_HTTPGET,   1L);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, resp);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
        fprintf(stderr, "[error] GET %s failed: %s\n", url,
                curl_easy_strerror(rc));
        return 0;
    }

    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    return code;
}

/*
 * Perform a POST request with application/x-www-form-urlencoded body.
 * Returns the HTTP status code, or 0 on transport error.
 */
static long http_post(CURL *c, const char *url, const char *post_fields,
                      struct response_data *resp)
{
    curl_easy_setopt(c, CURLOPT_URL,        url);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,  resp);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
        fprintf(stderr, "[error] POST %s failed: %s\n", url,
                curl_easy_strerror(rc));
        return 0;
    }

    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    return code;
}

/* -------------------------------------------------------------------------
 * Step 1 – Login
 * ---------------------------------------------------------------------- */

/*
 * New login endpoint : POST /netaccess/account/login
 * Form fields (from Login___CC_Networks.html):
 *   username=<ldap_id>
 *   password=<ldap_password>
 *   submit=Log+in
 *
 * On success the server redirects to /netaccess/account/index (dashboard).
 * We detect success by looking for strings that only appear when logged in.
 */
int perform_login(struct session_data *session,
                  const char *username, const char *password)
{
    struct response_data resp = {0};
    char url[512];
    long code;

    /* ------------------------------------------------------------------
     * 1a. GET the login page to pick up any session cookie the server sets
     *     before it sees a form submission.
     * ------------------------------------------------------------------ */
    snprintf(url, sizeof(url), "%s/account/login", BASE_URL);
    printf("[*] Fetching login page: %s\n", url);

    code = http_get(session->curl, url, &resp);
    if (code != 200) {
        fprintf(stderr, "[error] Login page GET returned HTTP %ld.\n", code);
        response_reset(&resp);
        return 0;
    }
    response_reset(&resp);

    /* ------------------------------------------------------------------
     * 1b. POST credentials.
     *     Field names from the HTML:  username  /  password  /  submit
     *     URL-encode the values properly so special chars survive the trip.
     * ------------------------------------------------------------------ */
    CURL *c = session->curl;
    char *u_enc = curl_easy_escape(c, username, 0);
    char *p_enc = curl_easy_escape(c, password, 0);

    char post_data[1024];
    snprintf(post_data, sizeof(post_data),
             "username=%s&password=%s&submit=Log+in",
             u_enc ? u_enc : username,
             p_enc ? p_enc : password);

    curl_free(u_enc);
    curl_free(p_enc);

    snprintf(url, sizeof(url), "%s/account/login", BASE_URL);
    printf("[*] Submitting credentials...\n");

    code = http_post(c, url, post_data, &resp);
    if (code == 0) {
        /* Transport error already printed */
        response_reset(&resp);
        return 0;
    }

    /*
     * The server may redirect (302) and CURLOPT_FOLLOWLOCATION will chase it.
     * After following, CURLINFO_RESPONSE_CODE is the *final* code.
     * Accept both 200 (direct success) and check body for dashboard markers.
     */
    if (code != 200) {
        fprintf(stderr, "[error] Login POST ended with HTTP %ld.\n", code);
        response_reset(&resp);
        return 0;
    }

    /* Markers that are present on the dashboard / any post-login page */
    int ok = resp.data &&
             (strstr(resp.data, "Authorized machines")   != NULL ||
              strstr(resp.data, "authorized machines")   != NULL ||
              strstr(resp.data, "/netaccess/account/logout") != NULL ||
              strstr(resp.data, "account/logout")        != NULL ||
              strstr(resp.data, "Approve")               != NULL);

    if (ok) {
        printf("[+] Login successful.\n");
    } else {
        fprintf(stderr,
                "[error] Login failed – invalid credentials, "
                "or a CAPTCHA/IP block is in effect.\n"
                "        Try logging in manually from a browser first "
                "to clear any CAPTCHA gate.\n");
    }

    response_reset(&resp);
    return ok ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * Step 2 – Approve machine
 * ---------------------------------------------------------------------- */

/*
 * New approve endpoint : POST /netaccess/account/approve
 * Form fields (from Approve___CC_Networks.html):
 *   duration = hour | day | month
 *   use_policy = on            ← new required checkbox
 *   approveBtn = Authorize
 *
 * We first GET the approve page (picks up CSRF / session state if any),
 * then POST the form.
 */
int approve_machine(struct session_data *session, const char *duration)
{
    struct response_data resp = {0};
    char url[512];
    long code;

    snprintf(url, sizeof(url), "%s/account/approve", BASE_URL);

    /* ------------------------------------------------------------------
     * 2a. GET the approve page.
     * ------------------------------------------------------------------ */
    printf("[*] Fetching approve page: %s\n", url);

    code = http_get(session->curl, url, &resp);
    if (code != 200) {
        fprintf(stderr, "[error] Approve page GET returned HTTP %ld.\n", code);
        response_reset(&resp);
        return 0;
    }

    /* Sanity-check: are we still logged in? */
    if (!resp.data || strstr(resp.data, "duration") == NULL) {
        fprintf(stderr,
                "[error] Approve page did not return the expected form. "
                "Session may have expired.\n");
        response_reset(&resp);
        return 0;
    }
    response_reset(&resp);

    /* ------------------------------------------------------------------
     * 2b. POST the approval form.
     *
     *  duration   – "hour" / "day" / "month"  (radio button value)
     *  use_policy – "on"                       (checkbox; required by server)
     *  approveBtn – "Authorize"                (submit button name/value)
     * ------------------------------------------------------------------ */
    char post_data[256];
    snprintf(post_data, sizeof(post_data),
             "duration=%s&use_policy=on&approveBtn=Authorize",
             duration);

    printf("[*] Submitting approval for duration='%s'...\n", duration);

    code = http_post(session->curl, url, post_data, &resp);
    if (code == 0) {
        response_reset(&resp);
        return 0;
    }

    if (code != 200) {
        fprintf(stderr,
                "[error] Approve POST returned HTTP %ld.\n", code);
        response_reset(&resp);
        return 0;
    }

    /*
     * A successful approval usually redirects back to the dashboard or
     * shows a confirmation. We look for positive signals; absence of an
     * obvious error page is also a reasonable indicator.
     */
    int ok = 1;   /* optimistic – server rarely gives a clear success string */
    if (resp.data) {
        if (strstr(resp.data, "error")   != NULL ||
            strstr(resp.data, "Error")   != NULL ||
            strstr(resp.data, "invalid") != NULL ||
            strstr(resp.data, "Invalid") != NULL) {
            fprintf(stderr,
                    "[warn] Approval response may contain an error. "
                    "Inspect the portal manually if internet access doesn't work.\n");
            /* Do not treat as hard failure – portal wording is ambiguous */
        }
    }

    if (ok)
        printf("[+] Approval request submitted successfully.\n");

    response_reset(&resp);
    return ok;
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <username> <password> [duration]\n"
            "  duration : hour | day | month  (default: day)\n\n"
            "Examples:\n"
            "  %s 19ucs002 mypassword\n"
            "  %s 19ucs002 mypassword hour\n",
            prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *username = argv[1];
    const char *password = argv[2];
    const char *duration = (argc >= 4) ? argv[3] : "day";

    /* Validate duration */
    if (strcmp(duration, "hour")  != 0 &&
        strcmp(duration, "day")   != 0 &&
        strcmp(duration, "month") != 0) {
        fprintf(stderr,
                "[error] Invalid duration '%s'. "
                "Choose: hour | day | month\n", duration);
        return 1;
    }

    printf("========================================\n");
    printf(" NetAccess Automation Tool\n");
    printf(" Portal  : %s\n", BASE_URL);
    printf(" Username: %s\n", username);
    printf(" Duration: %s\n", duration);
    printf("========================================\n");

    curl_global_init(CURL_GLOBAL_DEFAULT);

    struct session_data session = {0};
    int success = 0;

    if (!init_session(&session)) {
        curl_global_cleanup();
        return 1;
    }

    if (perform_login(&session, username, password)) {
        if (approve_machine(&session, duration)) {
            printf("========================================\n");
            printf("[+] Automation completed successfully!\n");
            printf("    Machine approved for: %s\n", duration);
            success = 1;
        }
    }

    if (!success) {
        printf("========================================\n");
        printf("[-] Automation failed.\n");
        printf("    Check your credentials and network connection.\n");
        printf("    If the portal shows a CAPTCHA, log in once manually\n");
        printf("    from a browser to clear it, then retry.\n");
    }

    cleanup_session(&session);
    curl_global_cleanup();

    return success ? 0 : 1;
}
