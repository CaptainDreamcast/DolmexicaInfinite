[Remap]
x = x
y = y
z = z
a = a
b = b
c = c
s = s

[Defaults]
command.time = 15
command.buffer.time = 1

;-| Hypers |------------------------------------------------------

[Command]
name = "Paint_Flamethrower"
command = ~D, F, D, x+y+z
time = 25

[Command]
name = "Paint_Flamethrower"
command = ~D, B, D, x+y+z
time = 25

[Command]
name = "Police_Riot"
command = ~D, F, D, a+b+c
time = 25

[Command]
name = "Police_Riot"
command = ~D, B, D, a+b+c
time = 25

;-| Specials |------------------------------------------------------

[Command]
name = "Paint_Spray"
command = ~D, F, y
time = 15

[Command]
name = "Paint_Spray"
command = ~D, F, b
time = 15

[Command]
name = "Paint_Spray"
command = ~D, B, y
time = 15

[Command]
name = "Paint_Spray"
command = ~D, B, b
time = 15

[Command]
name = "Paint_Can_Throw"
command = ~D, F, x
time = 15

[Command]
name = "Paint_Can_Throw"
command = ~D, F, a
time = 15

[Command]
name = "Paint_Can_Throw"
command = ~D, B, x
time = 15

[Command]
name = "Paint_Can_Throw"
command = ~D, B, a
time = 15

;-| Dir + Button |---------------------------------------------------------

[Command]
name = "Uppercut"
command = ~D, F, z
time = 15

[Command]
name = "Uppercut"
command = ~D, B, z
time = 15

[Command]
name = "Roundhouse_Kick"
command = ~D, F, c
time = 15

[Command]
name = "Roundhouse_Kick"
command = ~D, B, c
time = 15

;-| Single Button |---------------------------------------------------------
[Command]
name = "a"
command = a
time = 1

[Command]
name = "b"
command = b
time = 1

[Command]
name = "c"
command = c
time = 1

[Command]
name = "x"
command = x
time = 1

[Command]
name = "y"
command = y
time = 1

[Command]
name = "z"
command = z
time = 1

[Command]
name = "start"
command = s
time = 1

;-| Double Tap |-----------------------------------------------------------
[Command]
name = "FF"     ;Required (do not remove)
command = F, F
time = 10

[Command]
name = "BB"     ;Required (do not remove)
command = B, B
time = 10

;-| Button Combination |-----------------------------------------------
[Command]
name = "recovery";Required (do not remove)
command = x+y
time = 1

;-| Hold Dir |--------------------------------------------------------------
[Command]
name = "holdC";Required (do not remove)
command = /$c
time = 1

[Command]
name = "holdfwd";Required (do not remove)
command = /$F
time = 1

[Command]
name = "holdback";Required (do not remove)
command = /$B
time = 1

[Command]
name = "holdup" ;Required (do not remove)
command = /$U
time = 1

[Command]
name = "holddown";Required (do not remove)
command = /$D
time = 1

[Statedef -1]

[State -1, SSS]
type = ChangeState
value = 555
triggerall = command = "Paint_Flamethrower"
trigger1 = statetype = S
trigger1 = power >= 1000
trigger1 = ctrl

[State -1, SSS]
type = ChangeState
value = 666
triggerall = command = "Police_Riot"
trigger1 = statetype = S
trigger1 = power >= 3000
trigger1 = ctrl

[State -1, PGX]
type = ChangeState
value = 530
triggerall = command = "Uppercut"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, PGX]
type = ChangeState
value = 532
triggerall = command = "Roundhouse_Kick"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, PGY]
type = ChangeState
value = 535
triggerall = command = "Paint_Spray"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, PGY]
type = ChangeState
value = 537
triggerall = command = "Paint_Can_Throw"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, Run Fwd]
type = ChangeState
value = 100
trigger1 = command = "FF"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, Run Back]
type = ChangeState
value = 105
trigger1 = command = "BB"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, Taunt]
type = ChangeState
value = 195
triggerall = command = "start"
trigger1 = statetype != A
trigger1 = ctrl

[State -1, SLK]
type = ChangeState
value = 200
triggerall = command = "a"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, Pen]
type = ChangeState
value = 207
triggerall = command = "b"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, PGX]
type = ChangeState
value = 531
triggerall = command = "c"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, SLK]
type = ChangeState
value = 215
triggerall = command = "x"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, SLK]
type = ChangeState
value = 210
triggerall = command = "y"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, SLK]
type = ChangeState
value = 217
triggerall = command = "z"
trigger1 = statetype = S
trigger1 = ctrl

[State -1, CLK]
type = ChangeState
value = 300
triggerall = command = "a"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, CSK]
type = ChangeState
value = 307
triggerall = command = "b"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, CSK]
type = ChangeState
value = 305
triggerall = command = "c"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, CLP]
type = ChangeState
value = 310
triggerall = command = "x"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, CMP]
type = ChangeState
value = 315
triggerall = command = "y"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, CSP]
type = ChangeState
value = 317
triggerall = command = "z"
trigger1 = statetype = C
trigger1 = ctrl

[State -1, ALK]
type = ChangeState
value = 400
triggerall = command = "a"
trigger1 = statetype = A
trigger1 = ctrl

[State -1, AMK]
type = ChangeState
value = 405
triggerall = command = "b"
trigger1 = statetype = A
trigger1 = ctrl

[State -1, AMK]
type = ChangeState
value = 407
triggerall = command = "c"
trigger1 = statetype = A
trigger1 = ctrl

[State -1, ALP]
type = ChangeState
value = 410
triggerall = command = "x"
trigger1 = statetype = A
trigger1 = ctrl

[State -1, ALP]
type = ChangeState
value = 415
triggerall = command = "y"
trigger1 = statetype = A
trigger1 = ctrl

[State -1, ALP]
type = ChangeState
value = 417
triggerall = command = "z"
trigger1 = statetype = A
trigger1 = ctrl

[State 0, PlaySnd]
type = PlaySnd
trigger1 = time = 0
trigger1 = stateno = 40
value = 8008,0+random%5
volume = 255
channel = 0

[State 0, PlaySnd]
type = PlaySnd
trigger1 = time = 0
trigger1 = stateno = 5000 || stateno = 5010 || stateno = 5020 || stateno = 5030 || stateno = 5070
value = 605,0+random%7
volume = 255
channel = 0