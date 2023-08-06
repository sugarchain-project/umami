# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libsugarchain_cli*         | RPC client functionality used by *sugarchain-cli* executable |
| *libsugarchain_common*      | Home for common functionality shared by different executables and libraries. Similar to *libsugarchain_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libsugarchain_consensus*   | Stable, backwards-compatible consensus functionality used by *libsugarchain_node* and *libsugarchain_wallet* and also exposed as a [shared library](../shared-libraries.md). |
| *libsugarchainconsensus*    | Shared library build of static *libsugarchain_consensus* library |
| *libsugarchain_kernel*      | Consensus engine and support library used for validation by *libsugarchain_node* and also exposed as a [shared library](../shared-libraries.md). |
| *libsugarchainqt*           | GUI functionality used by *sugarchain-qt* and *sugarchain-gui* executables |
| *libsugarchain_ipc*         | IPC functionality used by *sugarchain-node*, *sugarchain-wallet*, *sugarchain-gui* executables to communicate when [`--enable-multiprocess`](multiprocess.md) is used. |
| *libsugarchain_node*        | P2P and RPC server functionality used by *sugarchaind* and *sugarchain-qt* executables. |
| *libsugarchain_util*        | Home for common functionality shared by different executables and libraries. Similar to *libsugarchain_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libsugarchain_wallet*      | Wallet functionality used by *sugarchaind* and *sugarchain-wallet* executables. |
| *libsugarchain_wallet_tool* | Lower-level wallet functionality used by *sugarchain-wallet* executable. |
| *libsugarchain_zmq*         | [ZeroMQ](../zmq.md) functionality used by *sugarchaind* and *sugarchain-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. Exceptions are *libsugarchain_consensus* and *libsugarchain_kernel* which have external interfaces documented at [../shared-libraries.md](../shared-libraries.md).

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`libsugarchain_*_SOURCES`](../../src/Makefile.am) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libsugarchain_node* code lives in `src/node/` in the `node::` namespace
  - *libsugarchain_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libsugarchain_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libsugarchain_util* code lives in `src/util/` in the `util::` namespace
  - *libsugarchain_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

sugarchain-cli[sugarchain-cli]-->libsugarchain_cli;

sugarchaind[sugarchaind]-->libsugarchain_node;
sugarchaind[sugarchaind]-->libsugarchain_wallet;

sugarchain-qt[sugarchain-qt]-->libsugarchain_node;
sugarchain-qt[sugarchain-qt]-->libsugarchainqt;
sugarchain-qt[sugarchain-qt]-->libsugarchain_wallet;

sugarchain-wallet[sugarchain-wallet]-->libsugarchain_wallet;
sugarchain-wallet[sugarchain-wallet]-->libsugarchain_wallet_tool;

libsugarchain_cli-->libsugarchain_util;
libsugarchain_cli-->libsugarchain_common;

libsugarchain_common-->libsugarchain_consensus;
libsugarchain_common-->libsugarchain_util;

libsugarchain_kernel-->libsugarchain_consensus;
libsugarchain_kernel-->libsugarchain_util;

libsugarchain_node-->libsugarchain_consensus;
libsugarchain_node-->libsugarchain_kernel;
libsugarchain_node-->libsugarchain_common;
libsugarchain_node-->libsugarchain_util;

libsugarchainqt-->libsugarchain_common;
libsugarchainqt-->libsugarchain_util;

libsugarchain_wallet-->libsugarchain_common;
libsugarchain_wallet-->libsugarchain_util;

libsugarchain_wallet_tool-->libsugarchain_wallet;
libsugarchain_wallet_tool-->libsugarchain_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class sugarchain-qt,sugarchaind,sugarchain-cli,sugarchain-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Consensus* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libsugarchain_wallet* and *libsugarchain_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libsugarchain_consensus* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libsugarchain_util* should also be a standalone dependency that any library can depend on, and it should not depend on other internal libraries.

- *libsugarchain_common* should serve a similar function as *libsugarchain_util* and be a place for miscellaneous code used by various daemon, GUI, and CLI applications and libraries to live. It should not depend on anything other than *libsugarchain_util* and *libsugarchain_consensus*. The boundary between _util_ and _common_ is a little fuzzy but historically _util_ has been used for more generic, lower-level things like parsing hex, and _common_ has been used for sugarchain-specific, higher-level things like parsing base58. The difference between util and common is mostly important because *libsugarchain_kernel* is not supposed to depend on *libsugarchain_common*, only *libsugarchain_util*. In general, if it is ever unclear whether it is better to add code to *util* or *common*, it is probably better to add it to *common* unless it is very generically useful or useful particularly to include in the kernel.


- *libsugarchain_kernel* should only depend on *libsugarchain_util* and *libsugarchain_consensus*.

- The only thing that should depend on *libsugarchain_kernel* internally should be *libsugarchain_node*. GUI and wallet libraries *libsugarchainqt* and *libsugarchain_wallet* in particular should not depend on *libsugarchain_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be get able it from *libsugarchain_consensus*, *libsugarchain_common*, and *libsugarchain_util*, instead of *libsugarchain_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libsugarchainqt*, *libsugarchain_node*, *libsugarchain_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](`../../src/interfaces/`) abstract interfaces.

## Work in progress

- Validation code is moving from *libsugarchain_node* to *libsugarchain_kernel* as part of [The libsugarchainkernel Project #24303](https://github.com/sugarchain/sugarchain/issues/24303)
- Source code organization is discussed in general in [Library source code organization #15732](https://github.com/sugarchain/sugarchain/issues/15732)
