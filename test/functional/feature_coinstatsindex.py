#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test coinstatsindex across nodes.

Test that the values returned by gettxoutsetinfo are consistent
between a node running the coinstatsindex and a node without
the index.
"""

from decimal import Decimal

from test_framework.blocktools import (
    create_block,
    create_coinbase,
)

from test_framework.cdefs import COINBASE_MATURITY
from test_framework.messages import (
    COIN,
    CTransaction,
    CTxOut,
    FromHex,
)
from test_framework.script import (
    CScript,
    OP_FALSE
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_not_equal,
    assert_raises_rpc_error,
    connect_nodes,
    wait_until,
)


class CoinStatsIndexTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.supports_cli = False
        self.extra_args = [
            [],
            ["-coinstatsindex"]
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self._test_coin_stats_index()
        self._test_use_index_option()
        self._test_reorg_index()
        self._test_index_rejects_hash_serialized()
        self._test_init_index_after_reorg()

    @staticmethod
    def block_sanity_check(block_info):
        block_subsidy = 50
        assert_equal(
            block_info['prevout_spent'] + block_subsidy,
            block_info['new_outputs_ex_coinbase'] + block_info['coinbase'] + block_info['unspendable']
        )

    def sync_index_node(self):
        wait_until(lambda: self.nodes[1].getindexinfo()['coinstatsindex']['synced'] is True)

    def _test_coin_stats_index(self):
        node = self.nodes[0]
        index_node = self.nodes[1]
        # Both none and muhash_testing options allow the usage of the index
        index_hash_options = ['none', 'muhash_testing', 'ecmh']

        def pop_hash_from_result(res):
            # Pop hash-related keys
            keys = ['muhash_testing', 'ecmh', 'ecmh_pubkey', 'hash_serialized_3']
            for to_pop in keys:
                res.pop(to_pop, None)

        def del_index_result_keys(res):
            # The fields 'block_info' and 'total_unspendable_amount' only exist on the index
            del res['block_info'], res['total_unspendable_amount']

        # Generate a normal transaction and mine it
        self.generate(node, COINBASE_MATURITY + 1)

        # Generate 3 txns that spend the same coin(s)
        addrs = [node.getnewaddress(), node.getnewaddress(), node.getnewaddress()]
        txns = []
        fees = []
        spends = []
        outs = []
        for addr in addrs:
            txid = node.sendtoaddress(addr, 1.0, "", "", False, 1, True)
            txns.append(txid)
            tx = node.getrawtransaction(txid, 2)
            fees.append(Decimal(tx['fee']))
            spends.append(sum(Decimal(inp['value']) for inp in tx['vin']))
            outs.append(sum(Decimal(outp['value']) for outp in tx['vout']))

        self.generate(node, 1)
        self.sync_blocks()

        self.log.info("Test that gettxoutsetinfo() output is consistent with or without coinstatsindex option")
        res0 = node.gettxoutsetinfo('none')

        # The fields 'disk_size' and 'transactions' do not exist on the index
        del res0['disk_size'], res0['transactions']

        for hash_option in index_hash_options:
            res1 = index_node.gettxoutsetinfo(hash_option)
            del_index_result_keys(res1)
            pop_hash_from_result(res1)
            # Everything left should be the same
            assert_equal(res1, res0)

        self.log.info("Test that gettxoutsetinfo() can get fetch data on specific heights with index")

        # Generate a new tip
        self.generate(node, 5)
        self.sync_blocks()

        for hash_option in index_hash_options:
            # Fetch old stats by height
            res2 = index_node.gettxoutsetinfo(hash_option, 102)
            del_index_result_keys(res2)
            pop_hash_from_result(res2)
            assert_equal(res0, res2)

            # Fetch old stats by hash
            res3 = index_node.gettxoutsetinfo(hash_option, res0['bestblock'])
            del_index_result_keys(res3)
            pop_hash_from_result(res3)
            assert_equal(res0, res3)

            # It does not work without coinstatsindex
            assert_raises_rpc_error(-8, "Querying specific block heights requires coinstatsindex",
                                    node.gettxoutsetinfo, hash_option, 102)

        self.log.info("Test gettxoutsetinfo() with index and verbose flag")

        for hash_option in index_hash_options:
            # Genesis block is unspendable
            res4 = index_node.gettxoutsetinfo(hash_option, 0)
            assert_equal(res4['total_unspendable_amount'], 50)
            assert_equal(res4['block_info'], {
                'unspendable': 50,
                'prevout_spent': 0,
                'new_outputs_ex_coinbase': 0,
                'coinbase': 0,
                'unspendables': {
                    'genesis_block': 50,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': 0
                }
            })
            self.block_sanity_check(res4['block_info'])

            # Test an older block height that included 3 normal txns
            res5 = index_node.gettxoutsetinfo(hash_option, 102)
            assert_equal(res5['total_unspendable_amount'], 50)
            assert_equal(res5['block_info'], {
                'unspendable': 0,
                'prevout_spent': sum(spends),
                'new_outputs_ex_coinbase': sum(outs) ,
                'coinbase': Decimal('50') + sum(fees),
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': 0,
                }
            })
            self.block_sanity_check(res5['block_info'])


        # Generate and send a normal tx with two outputs
        addr1 = node.validateaddress(node.get_deterministic_priv_key().address)['address']
        txid1 = node.sendtoaddress(addr1, Decimal('21.00000200'), "", "", False, 1, True)
        tx1 = node.getrawtransaction(txid1, 2)
        self.log.info(tx1)
        # Find the right position of the 21 BCH output
        out_n = None
        val_out_n = None
        fees = [Decimal(tx1['fee'])]
        spends = []
        outs = []
        for i, outp in enumerate(tx1['vout']):
            outs.append(Decimal(outp['value']))
            if outp['scriptPubKey']['addresses'][0] == addr1:
                out_n = i
                val_out_n = Decimal(outp['value'])
                assert val_out_n == Decimal('21.00000200')
        if out_n is None or val_out_n is None:
            raise RuntimeError("Could not find output N")
        for inp in tx1['vin']:
            spends.append(Decimal(inp['value']))

        # Generate and send another tx with a 21 BCH OP_RETURN output (which is unspendable)
        tx2 = FromHex(CTransaction(), node.createrawtransaction([{"txid": txid1, "vout": out_n}], [{"data": "ff"}]))
        tx2.vout[0].nValue = int(val_out_n * COIN - 200)  # Modify OP_RETURN to lock the 21 BCH amount
        fees.append(Decimal(200) / COIN)
        spends.append(val_out_n)
        res = node.signrawtransactionwithkey(tx2.serialize().hex(), [node.get_deterministic_priv_key().key])
        assert_equal(res['complete'], True)
        tx2 = FromHex(CTransaction(), res['hex'])
        tx2.rehash()
        tx2_val = Decimal(tx2.vout[0].nValue) / COIN
        assert_equal(tx2_val, 21)
        tx2_id = node.sendrawtransaction(tx2.serialize().hex())
        assert_equal(tx2_id, tx2.hash)

        # Include both txs in a block
        self.generate(self.nodes[0], 1)
        self.sync_blocks()

        for hash_option in index_hash_options:
            # Check all amounts were registered correctly
            res6 = index_node.gettxoutsetinfo(hash_option, 108)
            assert_equal(res6['total_unspendable_amount'], 71)
            assert_equal(res6['block_info'], {
                'unspendable': tx2_val,
                'prevout_spent': sum(spends),
                'new_outputs_ex_coinbase': sum(outs),
                'coinbase': Decimal('50') + sum(fees),
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': tx2_val,
                    'unclaimed_rewards': 0,
                }
            })
            self.block_sanity_check(res6['block_info'])

        # Create a coinbase that does not claim full subsidy and also
        # has two outputs
        cb = create_coinbase(109, nValue=35*COIN)
        cb.vout.append(CTxOut(5 * COIN, CScript([OP_FALSE])))
        cb.rehash()

        # Generate a block that includes previous coinbase
        tip = self.nodes[0].getbestblockhash()
        block_time = self.nodes[0].getblockheader(tip)['time'] + 1
        block = create_block(int(tip, 16), cb, block_time)
        block.solve()
        self.nodes[0].submitblock(block.serialize().hex())
        self.sync_all()

        for hash_option in index_hash_options:
            res7 = index_node.gettxoutsetinfo(hash_option, 109)
            assert_equal(res7['total_unspendable_amount'], 81)
            assert_equal(res7['block_info'], {
                'unspendable': 10,
                'prevout_spent': 0,
                'new_outputs_ex_coinbase': 0,
                'coinbase': 40,
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': 10
                }
            })
            self.block_sanity_check(res7['block_info'])

        for hash_type in ('muhash_testing', 'ecmh'):
            self.log.info(f"Test that the index is robust across restarts for hash_type: {hash_type}")

            res8 = index_node.gettxoutsetinfo(hash_type)
            self.restart_node(1, extra_args=self.extra_args[1])
            res9 = index_node.gettxoutsetinfo(hash_type)
            assert_equal(res8, res9)

            self.generate(index_node, 1)
            res10 = index_node.gettxoutsetinfo(hash_type)
            assert res8['txouts'] < res10['txouts']

            self.log.info("Test that the index works with -reindex")

            self.restart_node(1, extra_args=["-coinstatsindex", "-reindex"])
            self.sync_index_node()
            res11 = index_node.gettxoutsetinfo(hash_type)
            assert_equal(res11, res10)

            self.log.info("Test that the index works with -reindex-chainstate")

            self.restart_node(1, extra_args=["-coinstatsindex", "-reindex-chainstate"])
            self.sync_index_node()
            res12 = index_node.gettxoutsetinfo(hash_type)
            assert_equal(res12, res10)

        self.log.info("Test obtaining info for a non-existent block hash")
        assert_raises_rpc_error(-5, "Block not found", index_node.gettxoutsetinfo, hash_type="none",
                                hash_or_height="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                                use_index=True)

    def _test_use_index_option(self):
        self.log.info("Test use_index option for nodes running the index")

        connect_nodes(self.nodes[0], self.nodes[1])
        self.nodes[0].waitforblockheight(111)
        for hash_type in ('muhash_testing', 'ecmh'):
            res = self.nodes[0].gettxoutsetinfo(hash_type)
            option_res = self.nodes[1].gettxoutsetinfo(hash_type=hash_type, hash_or_height=None, use_index=False)
            del res['disk_size'], option_res['disk_size']
            assert_equal(res, option_res)

    def _test_reorg_index(self):
        self.log.info("Test that index can handle reorgs")

        # Generate two block, let the index catch up, then invalidate the blocks
        index_node = self.nodes[1]
        non_wallet_addr = index_node.get_deterministic_priv_key().address
        reorg_blocks = self.generatetoaddress(index_node, 2, non_wallet_addr)
        reorg_block = reorg_blocks[1]
        self.sync_index_node()
        res_invalid = index_node.gettxoutsetinfo('muhash_testing')
        index_node.invalidateblock(reorg_blocks[0])
        assert_equal(index_node.gettxoutsetinfo('muhash_testing')['height'], 111)

        # Add two new blocks
        block = self.generate(index_node, 2)[1]
        res = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=None, use_index=False)
        assert_equal(res['height'], 113)

        # Test that the result of the reorged block is not returned for its old block height
        res2 = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=113)
        assert_equal(res["bestblock"], block)
        assert_equal(res["muhash_testing"], res2["muhash_testing"])
        assert_not_equal(res["muhash_testing"], res_invalid["muhash_testing"])

        # Test that requesting reorged out block by hash is still returning correct results
        res_invalid2 = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=reorg_block)
        assert_equal(res_invalid2["muhash_testing"], res_invalid["muhash_testing"])
        assert_not_equal(res["muhash_testing"], res_invalid2["muhash_testing"])

        # Add another block, so we don't depend on reconsiderblock remembering which
        # blocks were touched by invalidateblock
        self.generate(index_node, 1)

        # Ensure that removing and re-adding blocks yields consistent results
        block = index_node.getblockhash(99)
        index_node.invalidateblock(block)
        index_node.reconsiderblock(block)
        res3 = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=113)
        assert_equal(res2, res3)

    def _test_index_rejects_hash_serialized(self):
        self.log.info("Test that the rpc raises if the legacy hash is passed with the index")

        index_node = self.nodes[1]
        tip_height = index_node.getblockheader(index_node.getbestblockhash())['height']
        assert_greater_than(tip_height, 99)

        msg = "hash_serialized_3 hash type cannot be queried for a specific block"

        for hash_type in ('hash_serialized_3',):
            assert_raises_rpc_error(-8, msg, index_node.gettxoutsetinfo, hash_type=hash_type,
                                    hash_or_height=tip_height - 2)

            for use_index in (True, False, None):
                assert_raises_rpc_error(-8, msg, index_node.gettxoutsetinfo, hash_type=hash_type,
                                        hash_or_height=tip_height - 2, use_index=use_index)

    def _test_init_index_after_reorg(self):
        self.log.info("Test a reorg while the index is deactivated")
        index_node = self.nodes[1]
        block = self.nodes[0].getbestblockhash()
        self.generate(index_node, 2)
        self.sync_index_node()

        # Restart without index
        self.restart_node(1, extra_args=[])
        assert_equal(index_node.getindexinfo("coinstatsindex"), {})
        connect_nodes(self.nodes[0], self.nodes[1])
        index_node.invalidateblock(block)
        non_wallet_addr = index_node.get_deterministic_priv_key().address
        self.generatetoaddress(index_node, 5, non_wallet_addr)
        res = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=None, use_index=False)
        res_ec = index_node.gettxoutsetinfo(hash_type='ecmh', hash_or_height=None, use_index=False)

        # Restart with index that still has its best block on the old chain
        self.restart_node(1, extra_args=self.extra_args[1])
        self.sync_index_node()
        res1 = index_node.gettxoutsetinfo(hash_type='muhash_testing', hash_or_height=None, use_index=True)
        res1_ec = index_node.gettxoutsetinfo(hash_type='ecmh', hash_or_height=None, use_index=True)
        assert_equal(res["muhash_testing"], res1["muhash_testing"])
        assert_equal(res_ec["ecmh"], res1_ec["ecmh"])


if __name__ == '__main__':
    CoinStatsIndexTest().main()
