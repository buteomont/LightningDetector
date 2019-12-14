static const char configPage[] PROGMEM = "<html>"
  "<head>"
  "<title>Lightning Logger Configuration</title>"
  "<style>body{background-color: #cccccc; Color: #000088; }</style>"
  "<script>function showSensitivity() {document.getElementById(\"sens\").innerHTML=document.getElementById(\"sensitivity\").value;}"
  "function showPitch() {document.getElementById(\"pitch\").innerHTML=document.getElementById(\"beepPitch\").value;}"
  " function endis(status)"
  "   {"
  "    var elements = document.getElementsByClassName(\"addrfield\");"
  "    for (var i = 0; i < elements.length; i++)" 
  "      {"
  "      elements[i].disabled = status;"
  "      }"
  "   }"
  "</script>"
  "</head>"
  "<body>"
  "<b><div style=\"text-align: center;\"><h3><a href=\"/\">Lightning Logger</a> Configuration</h3></div></b>"
  "<br><div id=\"message\">%s</div><br>"
  "Firmware Version: %s<br><br>"
  "<form action=\"/configure\" method=\"post\">"
  "SSID<span style=\"color:Crimson\">*</span>: <input type=\"text\" name=\"ssid\" value=\"%s\" maxlength=\"32\"><br>"
  "Password<span style=\"color:Crimson\">*</span>: <input type=\"password\" name=\"pword\" value=\"%s\" maxlength=\"255\"><br>"
  "mDNS<span style=\"color:Crimson\">*</span>: <input type=\"text\" name=\"mdns\" value=\"%s\" maxlength=\"64\">.local<br>"
  "<br>"
  "<input type=\"checkbox\" name=\"Static\" value=\"true\"%s onchange=\"{endis(!this.checked);}\">Static Address<span style=\"color:Crimson\">*</span>"
  "<br>"
  "<table border=0>"
  " <tr><td>Address<span style=\"color:Crimson\">*</span>:</td>"
  "    <td nowrap>"
  "    <input type=\"text\" name=\"addrOctet0\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\"  disabled />."
  "    <input type=\"text\" name=\"addrOctet1\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />."
  "    <input type=\"text\" name=\"addrOctet2\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />."
  "    <input type=\"text\" name=\"addrOctet3\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />"
  "    </td></tr>"
  " <tr><td>Gateway<span style=\"color:Crimson\">*</span>:</td>"
  "    <td nowrap>"
  "    <input type=\"text\" name=\"gwOctet0\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\"  disabled />."
  "    <input type=\"text\" name=\"gwOctet1\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />."
  "    <input type=\"text\" name=\"gwOctet2\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />."
  "    <input type=\"text\" name=\"gwOctet3\" value=\"%d\" size=\"3\" maxlength=\"3\" class=\"addrfield\" disabled />"
  "    </td></tr>"
  "  </table>"
  "<br><br>"
  "Time Zone Offset<span style=\"color:Crimson\">*</span>:&nbsp; <input type=\"number\" name=\"timezone\" value=\"%d\" min=\"-23\" max=\"23\"> hours"
  "<br><br>"
  "Sensitivity: <sup><span style=\"font-size: smaller;\">(Max)</span></sup> "
  " <input type=\"range\" name=\"sensitivity\" id=\"sensitivity\" value=\"%d\" min=\"1\" max=\"25\" step=\"1\""
  " oninput=\"showSensitivity()\">"
  " <sup><span style=\"font-size: smaller;\">(Min)</span></sup>"
  " <div align=\"left\" style=\"display: inline; \" id=\"sens\"></div>"
  "<br><br>"
  "<input type=\"checkbox\" name=\"beep\" value=\"true\"%s>"
  "Beep on detection"
  "<br>"
  "Pitch: <input type=\"range\" name=\"beepPitch\" id=\"beepPitch\" value=\"%d\" min=\"1600\" max=\"2200\" step=\"1\" oninput=\"showPitch()\" >&nbsp;"
  "<div align=\"left\" style=\"display: inline;\" id=\"pitch\"></div>&nbsp;Hz"
  "<br><br>"
  "<input type=\"submit\" name=\"Update Configuration\" value=\"Update\">"
  "</form>"
  "<br><br>"
  "<span style=\"color:Crimson\">*</span> Changing these will reset the device and erase the strike log."
  "<script>"
  " endis(!document.getElementsByName(\"Static\")[0].checked);"
  " showSensitivity();"
  " showPitch();"
  " </script>"
  "</body>"
  "</html>"
  ;
    
//configBuf is defined globally to keep it off the stack.
char configBuf[4096]; 
