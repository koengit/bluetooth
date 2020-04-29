
--------------------------------------------------------------------------------

type Agent = Int

{-
-- "C" API:
onReceive :: (Agent -> Msg -> IO ()) -> IO ()
send      :: Agent -> Msg -> IO ()
-}

--------------------------------------------------------------------------------
-- alternative 1

data Input msg = Receive Agent msg

data Output msg = Send Agent msg

-- does not work, one MUST send a message in the same clock cycle

--------------------------------------------------------------------------------
-- alternative 2

data Input msg = Receive Agent msg

data Output msg = Send Agent msg | Skip

-- does not work, one CAN ONLY send at most 1 message in the same clock cycle

--------------------------------------------------------------------------------
-- alternative 3

data Input msg = Receive Agent msg

data Output msg = Send [(Agent,msg)]

-- works, but no guarantees about size of list

--------------------------------------------------------------------------------
-- alternative 4

data Input msg = Receive Agent msg | Tick

data Output msg = Send Agent msg | Skip

-- works, but no guarantees about how many messages will go out before "resting"
-- (really, same problem as alt. 3)

--------------------------------------------------------------------------------

type Agent = Int

{-
-- "C" API:
onConnect    :: (Agent -> IO ()) -> IO ()
onReceive    :: (Msg -> IO ()) -> IO ()
onDisconnect :: (IO ()) -> IO ()
send         :: Msg -> IO ()
-}

--------------------------------------------------------------------------------
-- alternative 1

data Input msg
  = Connect Agent
  | Receive msg
  | Disconnect

data Output msg
  = Send [(Agent msg)]

-- works, but is only REACTIVE (just like above)

--------------------------------------------------------------------------------
-- alternative 2

data Input msg
  = Connect Agent
  | Receive msg
  | Disconnect
  | Tick

data Output msg
  = Send [(Agent msg)]

-- works (?)

--------------------------------------------------------------------------------

{-
-- NOTES

- no global variables are needed, the Lustre program takes care of this itself
-}




