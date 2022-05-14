# Open-Source Code Base for Debugging and Avoiding Peripheral Energy Bugs in Batteryless, Energy-Harvesting Systems

This is the open-source repository that demonstrates the Pudu flow for debugging
batteryless systems.  This repository includes all of the code used in the paper
evaluation, and we will add quality-of-life features (automatic setup scripts,
extra documentation, etc) after the work is accepted.

## Important Pieces

- ./PuduStatic: contains the llvm pass used to output typestate information used
  by PuduStatic to warn the programmer about potential device-bricking energy
  bugs

- ./ext/libpudu: contains the PuduDynamic hooks to remove performance bugs

- ./src: contains the bugs tested in the Bug Case Study section, the performance
  tests, and the robot case study.

- ./BugReports: contains the output of the PuduStatic pass on each of the bugs we
test in the paper
