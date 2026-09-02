epicsEnvSet("PREFIX",    "Lab{GeRM}")
epicsEnvSet("PORT",      "GERM")

epicsEnvSet("NELM",      "192")

# MCA_NELM = NELM * 4096, TDC_NELM = NELM * 1024
epicsEnvSet("MCA_NELM",  "786432")
epicsEnvSet("TDC_NELM",  "196608")

epicsEnvSet("ZYNQ_MAN_IP",   "172.16.0.211")
epicsEnvSet("ZYNQ_DATA_IP",  "172.16.0.212")
