
import Lustre
import qualified Data.Map as M

--------------------------------------------------------------------------------
-- general, very simple, Lustre bluetooth API

type Agent = Int

-- the clock ticks when we receive a message
data Input msg = Receive{ agent :: Agent, msg :: msg }
 deriving ( Eq, Ord, Show )

-- when the clock ticks we can send 0 or more messages
data Output msg = Send [(Agent,msg)]
 deriving ( Eq, Ord, Show )

send0     = Send []
send1 a m = Send [(a,m)]

--------------------------------------------------------------------------------
-- application API

-- a message to us reads or writes the state
data Msg = Read | Write String
 deriving ( Eq, Ord, Show )

str :: Msg -> String
str Read      = ""
str (Write s) = s

isWrite :: Msg -> Bool
isWrite (Write _) = True
isWrite _         = False

--------------------------------------------------------------------------------
-- simple server (same as the simple Zephyr example)

-- this server keeps track of a state (a String):
-- - anyone can Write the String
-- - anyone can Read the String

-- example: server0 [Receive 1 (Write "apa"), Receive 2 Read]

server0 :: Sig (Input Msg) -> Sig (Output String)
server0 inp =
  ifThenElse wr
    (con send0)                      -- if they write, we send nothing
    (send1 # (agent # inp) ¤ state)  -- if they read, we send the state to them
 where
  wr     = isWrite # (msg # inp)
  state  = ifThenElse wr (str # msg # inp) state'
  state' = con "" |-> pre state

--------------------------------------------------------------------------------
-- serving multiple agents independently

-- this server keeps track of a state (a String) for each Agent separately:
-- - anyone can Write their own String
-- - anyone can Read their own String

-- example: server [Receive 1 (Write "apa"), Receive 2 Read, Receive 1 Read]

server :: Sig (Input Msg) -> Sig (Output String)
server inp =
  ifThenElse wr
    (con send0)                     -- if they write, we send nothing
    (send1 # (agent # inp) ¤ ((?) # state ¤ (agent # inp)))
                                    -- if they read, we send their state to them
 where
  wr     = isWrite <$> msg <$> inp
  state  = ifThenElse wr (M.insert # (agent # inp) ¤ (str # (msg # inp)) ¤ state') state'
  state' = con M.empty |-> pre state

(?) :: Ord a => M.Map a String -> a -> String
m ? x = case M.lookup x m of
          Nothing -> ""
          Just s  -> s

-- IFC question: Is it guaranteed that noone can read someone else's String?

--------------------------------------------------------------------------------

