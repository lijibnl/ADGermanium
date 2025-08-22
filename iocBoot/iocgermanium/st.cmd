#!../../bin/linux-x86_64/germanium

#- SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#-
#- SPDX-License-Identifier: EPICS

#- You may have to change germanium to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/germanium.dbd"
germanium_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/germanium.db","user=liji")

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=liji"
