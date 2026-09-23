# Mobile Robot PID Controller

Differential-drive motor controller with quadrature encoder interrupt handling, PID speed control and JSON serial command parsing. It accepts linear/angular velocity commands or direct RPM targets and returns RPM/position feedback.

## Highlights

- Encoder ISR handling with `attachInterrupt`
- 20 Hz control loop
- Anti-windup integral limiting
- Differential-drive kinematics
- JSON serial interface
