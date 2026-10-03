# fileserver

cpp project that allows you to:
- create folders
- upload files
- download files
- delete files with a trash function
- see storage information (space, etc)

it includes a trash function which keeps files for 48 hours before deletion and storage info
it also shows your path and includes a safepath and other stuff

i use it on my personal debian server to which i connect to via tailscale to upload projects from my pc to my laptop
i made it as a learning project for a new library since i only did simple projects before like a guessing game with spammed if else
it was mostly for my need to share files and keep files easily. this way my family members can use my server instead of paid google drive, etc

i only coded the important functions in mainfunctions.cpp and vibecoded the functions for the trash and storage info
also the entire frontend is ai generated. i havent even touched a line of html or json
unlike the trash functions the frontend is fully ai generated and not so readable like the cpp code
i did very little for main.cpp too.

as i said this project was for learning and to solve a problem of mine. thats why i only touched the core stuff

the gui is very easy to use and convenient

## how to run

compile with make:
```bash
make
./fileserver
```
then you can open http://localhost:8080 in browser
