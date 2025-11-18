/// ============================================================================
/// SYNC DATA TYPES
/// ============================================================================
/// Each synced value has 3 versions: API, Local, and Self
/// Each version tracks its own lastModified timestamp
/// Phone app uses 2-way merge (either API or Local, not both)

class SyncBool {
  // API source (from cloud server)
  bool apiValue;
  int apiLastModified;

  // Local source (from device webserver)
  bool localValue;
  int localLastModified;

  // Self (app's current/stored value)
  bool value;
  int lastModified;

  SyncBool({
    this.apiValue = false,
    this.apiLastModified = 0,
    this.localValue = false,
    this.localLastModified = 0,
    this.value = false,
    this.lastModified = 0,
  });
}

class SyncDouble {
  // API source (from cloud server)
  double apiValue;
  int apiLastModified;

  // Local source (from device webserver)
  double localValue;
  int localLastModified;

  // Self (app's current/stored value)
  double value;
  int lastModified;

  SyncDouble({
    this.apiValue = 0.0,
    this.apiLastModified = 0,
    this.localValue = 0.0,
    this.localLastModified = 0,
    this.value = 0.0,
    this.lastModified = 0,
  });
}

class SyncString {
  // API source (from cloud server)
  String apiValue;
  int apiLastModified;

  // Local source (from device webserver)
  String localValue;
  int localLastModified;

  // Self (app's current/stored value)
  String value;
  int lastModified;

  SyncString({
    this.apiValue = '',
    this.apiLastModified = 0,
    this.localValue = '',
    this.localLastModified = 0,
    this.value = '',
    this.lastModified = 0,
  });
}
