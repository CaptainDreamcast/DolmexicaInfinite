;---------------------------------------------------------------------------
; Win state decider
[Statedef 180]
type = S

[State 180, 1]
type = ChangeState
trigger1 = Time = 0
value = 181

;---------------------------------------------------------------------------
; Win
[Statedef 181]
type = S
ctrl = 0
anim = 192
velset = 0,0

[State 181, 1]
type = NotHitBy
trigger1 = 1
value = SCA
time = 1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 233,2
volume = 255
channel = 0

;---------------------------------------------------------------------------
; Introduction
[Statedef 191]
type = S
ctrl = 0
anim = 190
velset = 0,0

[State 191, 2] ;Assert this until you want "round 1, fight" to begin
type = AssertSpecial
trigger1 = Time <= 200
flag = Intro

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 233,1
volume = 255
channel = -1

;---------------------------------------------------------------------------
;Run
[Statedef 100]
type    = S
physics = A
ctrl = 0
anim = 100
sprpriority = 1

[State 100, 1]
type = VelSet
trigger1 = Time = 0
x = const(velocity.run.fwd.x)
y = const(velocity.run.fwd.y)

[State 100, 2] ;Prevent run from canceling into walk
type = AssertSpecial
trigger1 = 1
flag = NoWalk

[State 100, 3] ;Prevent from turning
type = AssertSpecial
trigger1 = 1
flag = NoAutoTurn

[State 100, 4]
type = ChangeState
trigger1 = Pos y >= 1
value = 52

;---------------------------------------------------------------------------
; Run Back Land
[Statedef 106]
type    = S
physics = S
ctrl = 0
anim = 106

[State 106, 1]
type = VelSet
trigger1 = Time = 0
y = 0

[State 106, 2]
type = PosSet
trigger1 = Time = 0
y = 0

[State 106, 3]
type = MakeDust
trigger1 = Time = 2
pos = -5,-2
spacing = 1

[State 106, 4]
type = ChangeState
trigger1 = Time = 7
value = 0
ctrl = 1


;---------------------------------------------------------------------------
; Taunt
[Statedef 195]
type = S
ctrl = 0
anim = 195
velset = 0,0
movetype = I
physics = S
sprpriority = 2

[State 0, PowerAdd]
type = PowerAdd
trigger1 = Time = 119
value = 300

[State 195, 2]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 233,0
volume = 255
channel = 0

;---------------------------------------------------------------------------
; Kick
[Statedef 200]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 200                       ;Change animation (Def: no change)
poweradd = 25                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 2
value = 0,0
volume = 255
channel = -1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 2
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 30, 0                   ;Damage that move inflicts, guard damage
animtype = Light                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -35               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -7             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = High                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 200, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 200, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Low Kick
[Statedef 207]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 206                       ;Change animation (Def: no change)
poweradd = 25                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 40, 3                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = L                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 5, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 11, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -15, -20               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -2.5,0
air.velocity = -3,0
guard.velocity = -1.75
airguard.velocity = -2,0
air.type = Low                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.9,0           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Half-Moon Kick? I don't know what this is called...
[Statedef 531]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 531                       ;Change animation (Def: no change)
poweradd = 30                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 60, 6                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -3,-4
air.velocity = -3.5,-4.5
guard.velocity = -2.75
airguard.velocity = -2.5,-3.5
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Weak Punch
[Statedef 210]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 210                       ;Change animation (Def: no change)
poweradd = 15                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 20, 0                   ;Damage that move inflicts, guard damage
animtype = Light                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = M                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 6, 3                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = High               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 2             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -7             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = High                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Elbow Strike
[Statedef 215]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 215                       ;Change animation (Def: no change)
poweradd = 25                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 215, HitDef]
type = HitDef
trigger1 = AnimElem = 2
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 30, 0                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = M                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -15, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 1                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = High               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 3             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -7             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = High                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 2
value = 0,1
volume = 255
channel = -1

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Strong Punch
[Statedef 217]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 217                       ;Change animation (Def: no change)
poweradd = 30                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 217, HitDef]
type = HitDef
trigger1 = AnimElem = 3
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 55, 5                  ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = M                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 4, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -15, -85               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = High               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -9             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.9     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = High                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-2           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 2
value = 0,1
volume = 255
channel = -1

[State 0, VelAdd]
type = VelAdd
trigger1 = AnimElem = 2
x = 3

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Uppercut
[Statedef 530]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 530                       ;Change animation (Def: no change)
poweradd = 35                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 75, 9                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 5, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low               ;Type: High, Low, Trip (def: Normal)

ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -2,-2
air.velocity = -2.5,-2.5
guard.velocity = -1.25
airguard.velocity = -1.5,-1.5
air.type = Low                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 8008, 2
volume = 255
channel = 0

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Roundhouse Kick
[Statedef 532]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 532                       ;Change animation (Def: no change)
poweradd = 35                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 95, 11                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 0, 9                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)

ground.slidetime = 10             ;Time that the opponent slides back
ground.hittime  = 12             ;Time opponent is in hit state
ground.velocity = -8,0
air.velocity = -9.5,0
guard.velocity = -6.25
airguard.velocity = -7,0
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -6.7,0           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 14                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,1
volume = 255
channel = -1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
value = 8008, 1
volume = 255
channel = 0

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 11
ctrl = 1

;---------------------------------------------------------------------------
; Paint Spray
[Statedef 535]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 535                       ;Change animation (Def: no change)
poweradd = 0                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 205, HitDef]
type = HitDef
trigger1 = AnimElem = 1
trigger2 = AnimElem = 3
trigger3 = AnimElem = 5
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 10, 4                   ;Damage that move inflicts, guard damage
animtype = Light                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 3, 3                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -75               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Low               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -2             ;Velocity at which opponent is pushed
airguard.velocity = -0.7,-.2     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Low                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.0,-0.7           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 500,0
volume = 255
channel = 5

[State 205, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
value = 1

[State 205, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Paint Can Throw
[Statedef 537]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 537                       ;Change animation (Def: no change)
poweradd = 0                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 500,1
volume = 255
channel = 5

[State 0, Projectile]
type = Projectile
trigger1 = Time = 1
trigger1 = Random <= 333
projanim = 207
projscale = 1,1
projremovetime = -1
velocity = 6,0
remvelocity = 0,0
velmul = 1
projhits = 1
projpriority = 1
projsprpriority = -1
projedgebound = 40
projstagebound = 40
projheightbound = -240
offset = 5,-80
postype = p1         ;p2,front,back,left,right
projshadow = 0,0,0
supermovetime = 0
pausemovetime = 0
attr = S,SP        ;SCA,NA,SA,HA,NP,SP,HP,NT,ST,HT
hitflag = MAF        ;HLAFD+-
guardflag = MA         ;HLA
getpower = 0,0
givepower = 0,0
animtype = light          ;light,medium,hard,back,up,diagup
air.animtype = light
priority = 4,Hit   ;Hit,Miss,Dodge
damage = 15,2
pausetime = 5,5
;guard.pause,0
sparkno = 0
guard.sparkno = 0
sparkxy = 20,-10
hitsound = 5,0
guardsound = 0,0
ground.type = High

[State 0, Projectile]
type = Projectile
trigger1 = Time = 1
trigger1 = Random >= 333
projanim = 208
projscale = 1,1
projremovetime = -1
velocity = 6,0
remvelocity = 0,0
velmul = 1
projhits = 1
projpriority = 1
projsprpriority = -1
projedgebound = 40
projstagebound = 40
projheightbound = -240
offset = 5,-80
postype = p1         ;p2,front,back,left,right
projshadow = 0,0,0
supermovetime = 0
pausemovetime = 0
attr = S,SP        ;SCA,NA,SA,HA,NP,SP,HP,NT,ST,HT
hitflag = MAF        ;HLAFD+-
guardflag = MA         ;HLA
getpower = 0,0
givepower = 0,0
animtype = light          ;light,medium,hard,back,up,diagup
air.animtype = light
priority = 4,Hit   ;Hit,Miss,Dodge
damage = 15,2
pausetime = 5,5
;guard.pause,0
sparkno = 0
guard.sparkno = 0
sparkxy = 35,-10
hitsound = 5,0
guardsound = 0,0
ground.type = High

[State 0, Projectile]
type = Projectile
trigger1 = Time = 1
trigger1 = Random >= 666
projanim = 209
projscale = 1,1
projremovetime = -1
velocity = 6,0
remvelocity = 0,0
velmul = 1
projhits = 1
projpriority = 1
projsprpriority = -1
projedgebound = 40
projstagebound = 40
projheightbound = -240
offset = 5,-80
postype = p1         ;p2,front,back,left,right
projshadow = 0,0,0
supermovetime = 0
pausemovetime = 0
attr = S,SP        ;SCA,NA,SA,HA,NP,SP,HP,NT,ST,HT
hitflag = MAF        ;HLAFD+-
guardflag = MA         ;HLA
getpower = 0,0
givepower = 0,0
animtype = light          ;light,medium,hard,back,up,diagup
air.animtype = light
priority = 4,Hit   ;Hit,Miss,Dodge
damage = 15,2
pausetime = 5,5
;guard.pause,0
sparkno = 0
guard.sparkno = 0
sparkxy = 35,-10
hitsound = 5,0
guardsound = 0,0
ground.type = High

[State 537, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Paint Flamethrower
[Statedef 555]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 9                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 501                       ;Change animation (Def: no change)
poweradd = -1000                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, Pause]
type = Pause
trigger1 = AnimElem = 1
time = 40
movetime = 1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 700,1
volume = 255
channel = -1

[State 0, Explod]
type = Explod
trigger1 = Time = 1
anim = 600
pos = 44,-80
postype = p1  ;p2,front,back,left,right
facing = 1
vfacing = 1
bindtime = 1
removetime = -2
supermovetime = 30
pausemovetime = 30
scale = 1,1
sprpriority = 4
ontop = 1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
trigger2 = AnimElem = 7
trigger3 = AnimElem = 11
trigger4 = AnimElem = 12
trigger5 = AnimElem = 19
trigger6 = AnimElem = 23
trigger7 = AnimElem = 27
trigger8 = AnimElem = 31
trigger9 = AnimElem = 35
trigger10 = AnimElem = 39
trigger11 = AnimElem = 43
trigger12 = AnimElem = 47
trigger13 = AnimElem = 51
value = 6969,0
volume = 255
channel = 1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 3
trigger2 = AnimElem = 5
trigger3 = AnimElem = 7
trigger4 = AnimElem = 9
trigger5 = AnimElem = 10
trigger6 = AnimElem = 12
trigger7 = AnimElem = 14
trigger8 = AnimElem = 17
trigger9 = AnimElem = 19
trigger10 = AnimElem = 20
trigger11 = AnimElem = 21
trigger12 = AnimElem = 23
trigger13 = AnimElem = 24
trigger14 = AnimElem = 26
trigger15 = AnimElem = 28
trigger16 = AnimElem = 31
trigger17 = AnimElem = 33
trigger18 = AnimElem = 35
trigger19 = AnimElem = 37
trigger20 = AnimElem = 38
trigger21 = AnimElem = 39
trigger22 = AnimElem = 40
trigger23 = AnimElem = 42
trigger24 = AnimElem = 45
trigger25 = AnimElem = 47
trigger26 = AnimElem = 49
trigger27 = AnimElem = 51
trigger28 = AnimElem = 52
trigger29 = AnimElem = 54
trigger30 = AnimElem = 56
trigger31 = AnimElem = 59
trigger32 = AnimElem = 60
trigger33 = AnimElem = 61
trigger34 = AnimElem = 63
trigger35 = AnimElem = 65
trigger36 = AnimElem = 67
trigger37 = AnimElem = 69
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 8, 4                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 1, 30                 ;Time attacker pauses, time opponent shakes
sparkno = -1                      ;Spark anim no (Def: set above)
sparkxy = -5, -65               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
guardsound = 6, 0                ;Sound to play on guard
ground.type = High              ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -3             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = High                 ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-1           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 20                 ;Time before opponent regains control in air

[State 200, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1

;---------------------------------------------------------------------------
; Police Riot
[Statedef 666]
type    = S                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = S                      ;Physics: S-stand, C-crouch, A-air
juggle  = 9                      ;Number of air juggle points move takes
velset = 0,0                     ;Set velocity (x,y) (Def: no change)
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 549                       ;Change animation (Def: no change)
poweradd = -3000                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 24
value = 6969,1
volume = 255
channel = 1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 24
trigger2 = AnimElem = 25
trigger3 = AnimElem = 26
trigger4 = AnimElem = 27
trigger5 = AnimElem = 28
trigger6 = AnimElem = 29
trigger7 = AnimElem = 30
trigger8 = AnimElem = 31
trigger9 = AnimElem = 32
trigger10 = AnimElem = 33
trigger11 = AnimElem = 34
trigger12 = AnimElem = 35
trigger13 = AnimElem = 36
trigger14 = AnimElem = 37
trigger15 = AnimElem = 38
trigger16 = AnimElem = 39
trigger17 = AnimElem = 40
trigger18 = AnimElem = 41
trigger19 = AnimElem = 42
trigger20 = AnimElem = 43
trigger21 = AnimElem = 44
trigger22 = AnimElem = 45
trigger23 = AnimElem = 46
trigger24 = AnimElem = 47
trigger25 = AnimElem = 48
trigger26 = AnimElem = 49
trigger27 = AnimElem = 50
trigger28 = AnimElem = 51
trigger29 = AnimElem = 52
trigger30 = AnimElem = 53
trigger31 = AnimElem = 54
trigger32 = AnimElem = 55
trigger33 = AnimElem = 56
trigger34 = AnimElem = 57
trigger35 = AnimElem = 58
trigger36 = AnimElem = 59
trigger37 = AnimElem = 60
trigger34 = AnimElem = 61
trigger35 = AnimElem = 62
trigger36 = AnimElem = 63
trigger37 = AnimElem = 64
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 140, 70                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
hitflag = MAFD
guardflag = L
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 1, 30                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -35               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 2                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 10             ;Time that the opponent slides back
ground.hittime  = 15             ;Time opponent is in hit state
ground.velocity = 5             ;Velocity at which opponent is pushed
airguard.velocity = 3,-2     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = 6,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 17                 ;Time before opponent regains control in air

[State 200, ChangeState]
type = ChangeState
trigger1 = AnimTime = 0
value = 0
ctrl = 1