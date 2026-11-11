# Preface {.unnumbered}

This project exists to support Ondas & Ventos [@ondasventos], an operational wave forecast
system at the Federal University of Santa Catarina (UFSC). LabECO at UFSC runs WW3 operationally
for ReNOMO, the Brazilian national network for ocean observation and monitoring [@renomo].
ReNOMO started in 2023 with the CNPq/MCTI/Finep call 062/2022. More than 100 researchers from 31
Brazilian institutions take part in it [@renomo].

The system must run the wave model at each forecast cycle, and the run time of WW3 limits the
cycle. The system also plans to force the wave model with more than one wind model: the IFS of
ECMWF, the GFS of NCEP and Google's WeatherNext ⚠. Each wind model adds one full WW3 run, so an
ensemble of wind models multiplies the cost of the cycle. A WW3 that runs faster on GPUs and gives
the same bits makes these runs affordable without a change to the forecast. ww3-gpu and chaceon
work toward that WW3, and this note claims no speed-up.
