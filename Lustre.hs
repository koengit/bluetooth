module Lustre where

infixl 1 ¤
infixr 2 #

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

instance Num a => Num [a] where
  (+) = zipWith (+)
  (-) = zipWith (-)
  (*) = zipWith (*)
  abs    = map abs
  signum = map signum
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
-- nice alternative to zipWith, zipWith2, zipWith3, ... notation

-- Notation:
-- f # x         -- apply the function f to all elements of the stream x
-- f # x ¤ y ¤ z -- apply the 3-ary function f to all elements of the streams x, y, z

(#) :: (a -> b) -> Sig a -> Sig b
f # xs = map f xs

(¤) :: Sig (a -> b) -> Sig a -> Sig b
fs ¤ xs = zipWith ($) fs xs

--------------------------------------------------------------------------------

