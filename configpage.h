static const char configPage[] PROGMEM = "<html>"
    "<head>"
    "<title>Lightning Detector Configuration</title>"
    "<style>body{background-color: #cccccc; Color: #000088; }</style>"
    "<script>function showSensitivity() {document.getElementById(\"sens\").innerHTML=document.getElementById(\"sensitivity\").value;}</script>"
    "</head>"
    "<body>"
    "<b><div style=\"text-align: center;\"><h1><a href=\"/\">Lightning Detector</a> Configuration</h1></div></b>"
    "<br><div id=\"message\">%s</div><br>"
    "<form action=\"/configure\" method=\"post\">"
    "SSID: <input type=\"text\" name=\"ssid\" value=\"%s\" maxlength=\"32\"><br>"
    "Password: <input type=\"text\" name=\"pword\" value=\"%s\" maxlength=\"255\"><br>"
    "mDNS: <input type=\"text\" name=\"mdns\" value=\"%s\" maxlength=\"64\">.local<br>"
    "Sensitivity: <sup><span style=\"font-size: smaller;\">(Max)</span></sup> "
    " <input type=\"range\" name=\"sensitivity\" id=\"sensitivity\" value=\"%s\" min=\"1\" max=\"25\" step=\"1\""
    " onchange=\"showSensitivity()\">"
    " <sup><span style=\"font-size: smaller;\">(Min)</span></sup>"
    " <div align=\"left\" style=\"display: inline; \" id=\"sens\"></div>"
    "<br><br>"
    "<script> showSensitivity();</script>"
    "<input type=\"checkbox\" name=\"factory_reset\" value=\"reset\" "
    "onchange=\"if (this.checked) {alert('Checking this box will cause the configuration to be set to "
    "factory defaults and reset the detector! \\nOnce reset, you must connect your wifi to access point "
    "\\'lightning!\\' and browse to http://lightning.local to reconfigure it.');"
    "document.forms[0].action='/reset';}else document.forms[0].action='/configure'\">"
    "Factory Reset"
    "<br><br>"
    "<input type=\"submit\" name=\"Update Configuration\" value=\"Update\">"
    "</form>"
    "<script>"
    "var msg=new URLSearchParams(document.location.search.substring(1)).get(\"msg\");"
    "if (msg)"
    " document.getElementById(\"message\").innerHTML=msg;"
    "</script>"
    "</body>"
    "</html>"
    ;
    
//configBuf is defined globally to keep it off the stack.
char configBuf[3072]; //I don't know why it has to be twice the size
