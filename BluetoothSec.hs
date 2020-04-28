{-# LANGUAGE GADTs #-}

import Lustre
import qualified Data.Map as M

--------------------------------------------------------------------------------
-- general, very simple, Lustre bluetooth API

newtype Agent a = Agent Int

data Pack msg where
  Pack :: Agent a -> msg a -> Pack msg

type Input msg = Pack msg

type Output msg = [Pack msg]

--------------------------------------------------------------------------------
-- application API

data Msg a = Read | Write a

--------------------------------------------------------------------------------
-- serving multiple agents independently

-- example: server [Receive 1 (Write "apa"), Receive 2 Read, Receive 1 Read]

server :: Sig (Input Msg) -> Sig (Output String)
server inp = undefined

--------------------------------------------------------------------------------

