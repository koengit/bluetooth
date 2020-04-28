{-# LANGUAGE GADTs #-}

import Lustre
import qualified Data.Map as M

--------------------------------------------------------------------------------
-- BACKGROUND PROBLEM:

-- this server keeps track of a state (a String) for each Agent separately:
-- - anyone can Write their own String
-- - anyone can Read their own String

-- IFC question: Is it guaranteed that noone can read someone else's String?

-- PROPOSED SOLUTION: use types to tie Strings with the corresponding Agent?

--------------------------------------------------------------------------------
-- general, very simple, Lustre bluetooth API

newtype Agent a = ID Int

data Pack msg where
  Pack :: Agent a -> msg a -> Pack msg

type Input msg = Pack msg

type Output msg = [Pack msg]

-- in the above way, you can only ever send things to the corresponding agent

--------------------------------------------------------------------------------
-- application API

data Msg a = Read | Write a

--------------------------------------------------------------------------------
-- serving multiple agents independently

-- example: server [Receive 1 (Write "apa"), Receive 2 Read, Receive 1 Read]

server :: Sig (Input Msg) -> Sig (Output String)
server inp = undefined

--------------------------------------------------------------------------------

