;---------------------------------------------------------------------------
; Crouch Weak Kick
[Statedef 300]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = c                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 300                       ;Change animation (Def: no change)
poweradd = 15                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 20, 0                   ;Damage that move inflicts, guard damage
animtype = Light                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
hitflag = MAFD
guardflag = L
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 4, 6                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -18, -10               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low
ground.slidetime = 5
ground.hittime  = 8
ground.velocity = -2.4
airguard.velocity = -0.5,0
air.type = Trip
air.velocity = -0.8,0
air.hittime = 10

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Crouch Medium Kick
[Statedef 307]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = c                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 302                       ;Change animation (Def: no change)
poweradd = 20                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 45, 3                   ;Damage that move inflicts, guard damage
animtype = Hard                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
hitflag = MAFD
guardflag = L
priority = 5, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 7, 7                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -18, -10               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low              ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 6             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -4.75             ;Velocity at which opponent is pushed
airguard.velocity = -0.75,0     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.15,0           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponnt is pushed
air.hittime = 12                 ;Time before opponent regains control in ai

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 0, VelAdd]
type = VelAdd
trigger1 = AnimElem = 1
x = 6

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Crouch Strong Kick
[Statedef 305]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = c                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 303                       ;Change animation (Def: no change)
poweradd = 35                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 70, 6                   ;Damage that move inflicts, guard damage
animtype = Hard                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -22, -10               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip              ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 6             ;Time that the opponent slides back
ground.hittime  = 12             ;Time opponent is in hit state
ground.velocity = -6.25             ;Velocity at which opponent is pushed
airguard.velocity = -2.25,-.9     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -2.25,-2           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 14                 ;Time before opponent regains control in ai

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 2
value = 8008, 2
volume = 255
channel = 0

[State 0, VelAdd]
type = VelAdd
trigger1 = AnimElem = 1
x = 2

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Crouch Weak Punch
[Statedef 310]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = C                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 310                       ;Change animation (Def: no change)
poweradd = 15                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 20, 0                   ;Damage that move inflicts, guard damage
animtype = Light                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
pausetime = 5, 6
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -55               ;X-offset for the "hit spark" rel. to p2,
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low
ground.slidetime = 12
ground.hittime  = 10
ground.velocity = -2.4
airguard.velocity = -1.2,-.8
air.type = High
air.velocity = -1.4,-3
air.hittime = 12

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Crouch Medium Punch
[Statedef 315]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = C                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 315                       ;Change animation (Def: no change)
poweradd = 25                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 25, 0                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 4, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -15, -35               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 12
ground.hittime  = 10
ground.velocity = -2.4
airguard.velocity = -1.2,-.8
air.type = High
air.velocity = -1.4,-3
air.hittime = 12

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 0, VelAdd]
type = VelAdd
trigger1 = AnimElem = 1
x = 4.5

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Shoulder Strike
[Statedef 317]
type    = C                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = C                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 6,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 316                       ;Change animation (Def: no change)
poweradd = 35                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 300, HitDef]
type = HitDef
trigger1 = AnimElem = 3
attr = C, NA                     ;Attribute: Standing, Normal Attack
damage = 60, 4                   ;Damage that move inflicts, guard damage
animtype = Back                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -55               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 13
ground.hittime  = 16
ground.velocity = -4,-4
air.velocity = -5,-5
guard.velocity = -3
airguard.velocity = -4,-2
fall = 1
fall.recover = 0
fall.recovertime = 1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
value = 0,1
volume = 255
channel = -1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1