
import qualified Data.Map as M

infixl 1 ¤
infixr 2 #

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

--------------------------------------------------------------------------------
-- Lustre

type Sig a = [a]

con :: a -> Sig a
con x = repeat x

(|->) :: Sig a -> Sig a -> Sig a
(x:_) |-> (_:ys) = x:ys

pre :: Sig a -> Sig a
pre xs = undefined : xs

ifThenElse :: Sig Bool -> Sig a -> Sig a -> Sig a
ifThenElse = zipWith3 (\c a b -> if c then a else b)

-- Notation:
-- f # x         -- apply the function f to all elements of the stream x
-- f # x ¤ y ¤ z -- apply the 3-ary function f to all elements of the streams x, y, z

(#) :: (a -> b) -> Sig a -> Sig b
f # xs = map f xs

(¤) :: Sig (a -> b) -> Sig a -> Sig b
fs ¤ xs = zipWith ($) fs xs

instance Num a => Num [a] where
  (+) = zipWith (+)
  -- ...
  fromInteger n = con (fromInteger n)

(.||), (.&&) :: Sig Bool -> Sig Bool -> Sig Bool
(.&&) = zipWith (&&)
(.||) = zipWith (||)

nott :: Sig Bool -> Sig Bool
nott = map not

false, true :: Sig Bool
false = con False
true  = con True

--------------------------------------------------------------------------------

