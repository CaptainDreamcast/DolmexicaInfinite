;---------------------------------------------------------------------------
; Air Low Kick
[Statedef 400]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 400                       ;Change animation (Def: no change)
poweradd = 10                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,0
volume = 255
channel = -1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 20, 0                   ;Damage that move inflicts, guard damage
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
ground.type = High               ;Type: High, Low, Trip (def: Normal)
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
trigger2 = AnimElem = 4
value = 1

;---------------------------------------------------------------------------
; Air Medium Kick
[Statedef 405]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 405                       ;Change animation (Def: no change)
poweradd = 10                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,0
volume = 255
channel = -1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 25, 0                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
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
ground.type = High              ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -7             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Low                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -1.4,-3           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 200, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
trigger2 = AnimElem = 4
value = 1

;---------------------------------------------------------------------------
; Air Axe Kick
[Statedef 407]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 407                       ;Change animation (Def: no change)
poweradd = 50                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
value = 0,0
volume = 255
channel = -1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
value = 8008, 1
volume = 255
channel = 0

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 3
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 115, 15                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -35               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 7             ;Time that the opponent slides back
ground.hittime  = 12             ;Time opponent is in hit state
ground.velocity = -5             ;Velocity at which opponent is pushed
airguard.velocity = -1,1     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -2,12           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 14                 ;Time before opponent regains control in air

[State 200, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
trigger2 = AnimElem = 6
value = 1

;---------------------------------------------------------------------------
; Air Elbow Strike
[Statedef 410]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 410                       ;Change animation (Def: no change)
poweradd = 10                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,0
volume = 255
channel = -1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 25, 0                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 4, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 7, 7                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -7, -85               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = High               ;Type: High, Low, Trip (def: Normal)
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
trigger2 = AnimElem = 4
value = 1

;---------------------------------------------------------------------------
; Air Punch
[Statedef 415]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 415                       ;Change animation (Def: no change)
poweradd = 10                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 1
value = 0,0
volume = 255
channel = -1

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 1
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 15, 0                   ;Damage that move inflicts, guard damage
animtype = Medium                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 3, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 7, 7                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -85               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = High               ;Type: High, Low, Trip (def: Normal)
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
trigger2 = AnimElem = 4
value = 1

;---------------------------------------------------------------------------
; Air Cartwheel Thing
[Statedef 417]
type    = A                      ;State-type: S-stand, C-crouch, A-air, L-liedown
movetype= A                      ;Move-type: A-attack, I-idle, H-gethit
physics = A                      ;Physics: S-stand, C-crouch, A-air
juggle  = 1                      ;Number of air juggle points move takes
ctrl = 0                         ;Set ctrl (Def: no change)
anim = 417                       ;Change animation (Def: no change)
poweradd = 50                    ;Power to add (Def: 0)
sprpriority = 2                  ;Set p1's sprite layering priority to 2 (in front)

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 3
value = 0,0
volume = 255
channel = -1

[State 0, PlaySnd]
type = PlaySnd
trigger1 = AnimElem = 2
value = 8008, 0
volume = 255
channel = 0

[State 200, HitDef]
type = HitDef
trigger1 = AnimElem = 3
attr = S, NA                     ;Attribute: Standing, Normal Attack
damage = 120, 15                   ;Damage that move inflicts, guard damage
animtype = Heavy                 ;Animation type: Light, Medium, Heavy, Back (def: Light)
guardflag = MA                   ;Flags on how move is to be guarded against
hitflag = MAF                    ;Flags of conditions that move can hit
priority = 7, Hit                ;Attack priority: 0 (least) to 7 (most), 4 default
;Hit/Miss/Dodge type (Def: Hit)
pausetime = 8, 8                 ;Time attacker pauses, time opponent shakes
sparkno = 0                      ;Spark anim no (Def: set above)
sparkxy = -10, -35               ;X-offset for the "hit spark" rel. to p2,
;Y-offset for the spark rel. to p1
hitsound = 5, 0                  ;Sound to play on hit
guardsound = 6, 0                ;Sound to play on guard
ground.type = Trip               ;Type: High, Low, Trip (def: Normal)
ground.slidetime = 5             ;Time that the opponent slides back
ground.hittime  = 10             ;Time opponent is in hit state
ground.velocity = -7             ;Velocity at which opponent is pushed
airguard.velocity = -1.0,-.8     ;Guard velocity in air (def: (air.xvel*1.5, air.yvel/2))
air.type = Trip                  ;Type: High, Low, Trip (def: same as ground.type)
air.velocity = -2.4,10           ;X-velocity at which opponent is pushed,
;Y-velocity at which opponent is pushed
air.hittime = 12                 ;Time before opponent regains control in air

[State 200, CtrlSet]
type = CtrlSet
trigger1 = MoveHit
trigger2 = AnimElem = 6
value = 1