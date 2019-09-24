1/ Rename Time.h to _Time.h and replace includes
2/ Replace 
     if(!_client->connect(_host.c_str(), _port, _connectTimeout))
   by
     if(!_client->connect(_host.c_str(), _port)) {
   in .platformio\packages\framework-arduinoespressif32\libraries\HTTPClient\src\HTTPClient.cpp 