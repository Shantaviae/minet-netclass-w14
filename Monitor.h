struct MonitoringEvent : string {
  int Serialize() const;
  int Unserialize();
  int Print(ostream &) const;
}


MinetSend(monitorhandle,MonitoringEvent("Stuff is broken"));


  
