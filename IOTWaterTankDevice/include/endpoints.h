#ifndef ENDPOINTS_H
#define ENDPOINTS_H

// ============================================================================
// COMPLETE API ENDPOINTS LIST
// Updated with all backend routes
// ============================================================================

// ============================================================================
// AUTHENTICATION
// ============================================================================

#define API_AUTH                     "/api/auth"  // Better Auth endpoints (sign-in, sign-up, etc.)

// ============================================================================
// DEVICE AUTHENTICATION
// ============================================================================

#define API_DEVICE_LOGIN             "/api/device-auth/login"      // Device login (get JWT token)
#define API_DEVICE_HEARTBEAT         "/api/device-auth/heartbeat"  // Device heartbeat (JWT required)
#define API_DEVICE_VERIFY            "/api/device-auth/verify"     // Verify JWT token
#define API_DEVICE_REFRESH           "/api/device-auth/refresh"    // Refresh JWT token
#define API_DEVICE_REVOKE            "/api/device-auth/revoke"     // Revoke JWT token

// ============================================================================
// DEVICE DATA (IoT Devices)
// ============================================================================

#define API_DEVICE_CONFIG            "/api/device/config"          // GET/POST device configuration
#define API_DEVICE_CONTROL           "/api/device/control"         // POST control commands to device
#define API_DEVICE_TELEMETRY         "/api/device-telemetry"       // POST telemetry data

// Firmware Management
#define API_FIRMWARE_LATEST          "/api/device/firmware/latest"    // GET latest firmware info
#define API_FIRMWARE_DOWNLOAD        "/api/device/firmware/download"  // GET download firmware
#define API_FIRMWARE_DOWNLOAD_ID     "/api/device/firmware/download"  // GET download specific firmware (append /[firmwareId])

// ML Model Management
#define API_MLMODEL_LATEST           "/api/device/mlmodel/latest"   // GET latest ML model
#define API_MLMODEL_DOWNLOAD         "/api/device/mlmodel/download" // GET download ML model (append /[modelId])
#define API_MLMODELS_LIST            "/api/device/mlmodels"         // GET all ML models for device

// ============================================================================
// DEVICES MANAGEMENT (Dashboard)
// ============================================================================

#define API_DEVICES                  "/api/devices"                // GET all devices / POST create device
#define API_DEVICES_BY_ID            "/api/devices"                // GET/PATCH/DELETE device (append /[id])
#define API_DEVICES_RESET            "/api/devices"                // POST reset device data (append /[id]/reset)
#define API_DEVICES_BY_NAME          "/api/devices/by-name"        // GET device by name (append /[name])
#define API_DEVICES_CLAIM            "/api/devices/claim"          // POST claim unclaimed device
#define API_DEVICES_UNCLAIM          "/api/devices/unclaim"        // POST unclaim device
#define API_DEVICES_BIND             "/api/devices/bind"           // POST bind device to project
#define API_DEVICES_CHECK_OFFLINE    "/api/devices/check-offline"  // GET check and mark offline devices

// Migration utilities (admin only)
#define API_DEVICES_MIGRATE_TS       "/api/devices/migrate-timestamps"
#define API_DEVICES_MIGRATE_FORCE    "/api/devices/migrate-force-update"
#define API_DEVICES_REMOVE_DEFAULT   "/api/devices/remove-default-value"
#define API_DEVICES_ADD_STATUS       "/api/devices/add-status-field"

// ============================================================================
// PROJECTS
// ============================================================================

#define API_PROJECTS                 "/api/projects"               // GET all projects / POST create project
#define API_PROJECTS_BY_ID           "/api/projects"               // GET/PATCH/DELETE project (append /[id])
#define API_PROJECTS_SYNC_DEVICES    "/api/projects"               // POST sync project devices (append /[id]/sync-devices)

// Project Firmware Management
#define API_PROJECT_FIRMWARE         "/api/projects"               // [id]/firmware - GET/POST firmware
#define API_PROJECT_FIRMWARE_ID      "/api/projects"               // [id]/firmware/[firmwareId] - GET/PATCH/DELETE
#define API_PROJECT_FIRMWARE_DL      "/api/projects"               // [id]/firmware/[firmwareId]/download
#define API_PROJECT_FIRMWARE_FORCE   "/api/projects"               // [id]/firmware/[firmwareId]/force-update
#define API_PROJECT_FIRMWARE_ACTIVE  "/api/projects"               // [id]/firmware/active

// Project ML Models Management
#define API_PROJECT_MLMODELS         "/api/projects"               // [id]/mlmodels - GET/POST ML models
#define API_PROJECT_MLMODELS_ID      "/api/projects"               // [id]/mlmodels/[modelId] - GET/PATCH/DELETE
#define API_PROJECT_MLMODELS_DL      "/api/projects"               // [id]/mlmodels/[modelId]/download
#define API_PROJECT_MLMODELS_ACTIVE  "/api/projects"               // [id]/mlmodels/active

// ============================================================================
// ML SCRIPTS
// ============================================================================

#define API_MLSCRIPTS                "/api/mlscripts"              // GET all / POST create ML script
#define API_MLSCRIPTS_BY_ID          "/api/mlscripts"              // GET/PATCH/DELETE ML script (append /[id])
#define API_MLSCRIPTS_EXECUTE        "/api/mlscripts"              // POST execute ML script (append /[id]/execute)

// ============================================================================
// TELEMETRY HISTORY
// ============================================================================

#define API_TELEMETRY_HISTORY        "/api/telemetry-history"      // GET telemetry history

// ============================================================================
// USERS
// ============================================================================

#define API_USERS                    "/api/users"                  // GET all users / POST create user
#define API_USERS_BY_ID              "/api/users"                  // GET/PATCH/DELETE user (append /[id])
#define API_USERS_SEARCH             "/api/users/search"           // GET search users
#define API_USERS_STATUS             "/api/users/status"           // PATCH update user status
#define API_USERS_BULK               "/api/users/bulk"             // POST bulk user operations

// ============================================================================
// POSTS & CATEGORIES
// ============================================================================

#define API_POSTS                    "/api/posts"                  // GET all posts / POST create post
#define API_POSTS_BY_ID              "/api/posts"                  // GET/PATCH/DELETE post (append /[id])
#define API_POSTS_FEATURED           "/api/posts/featured"         // GET featured posts
#define API_POSTS_BY_SLUG            "/api/posts/slug"             // GET post by slug (append /[slug])
#define API_POSTS_BY_CATEGORY        "/api/posts/category"         // GET posts by category (append /[categoryId])
#define API_POSTS_RELATED            "/api/posts/related"          // GET related posts
#define API_POSTS_PUBLISH            "/api/posts"                  // PATCH publish post (append /[id]/publish)
#define API_POSTS_VIEWS              "/api/posts"                  // PATCH increment views (append /[id]/views)
#define API_POSTS_COMMENTS           "/api/posts"                  // GET/POST comments (append /[id]/comments)
#define API_POSTS_BULK               "/api/posts/bulk"             // POST bulk post operations
#define API_POSTS_BULK_TAGS          "/api/posts/bulk/tags"        // POST bulk tag operations
#define API_POSTS_BULK_STATUS        "/api/posts/bulk/status"      // POST bulk status update
#define API_POSTS_BULK_CATEGORY      "/api/posts/bulk/category"    // POST bulk category update

#define API_CATEGORIES               "/api/categories"             // GET all / POST create category
#define API_CATEGORIES_BY_ID         "/api/categories"             // GET/PATCH/DELETE category (append /[id])
#define API_CATEGORIES_BULK          "/api/categories/bulk"        // POST bulk category operations

// ============================================================================
// UTILITIES
// ============================================================================

#define API_TIME_SYNC                "/api/timeSync"               // GET server time
#define API_SEND_EMAIL               "/api/send-email"             // POST send email
#define API_SERVER                   "/api/server"                 // GET check superadmin status
#define API_SETUP_PYTHON             "/api/setup-python"           // POST setup Python environment
#define API_TERMINAL                 "/api/terminal"               // POST terminal operations

// Debug endpoints
#define API_DEBUG_CHECK_USER         "/api/debug/check-user"       // GET debug user check
#define API_DEBUG_SET_PASSWORD       "/api/debug/set-password"     // POST debug set password

// ============================================================================
// LEGACY COMPATIBILITY ALIASES (deprecated - use new endpoints above)
// ============================================================================

#define API_REGISTER                 API_DEVICE_LOGIN              // Legacy: Use login instead
#define API_CONFIG                   API_DEVICE_CONFIG             // Legacy alias
#define API_CONTROL                  API_DEVICE_CONTROL            // Legacy alias
#define API_TELEMETRY                API_DEVICE_TELEMETRY          // Legacy alias

#endif // ENDPOINTS_H
