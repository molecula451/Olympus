require('ts-node/register'); // eslint-disable-line
require('dotenv-flow').config(); // eslint-disable-line
const HDWalletProvider = require('truffle-hdwallet-provider'); // eslint-disable-line
const path = require('path');

const covContractsDir = path.join(process.cwd(), '.coverage_contracts');
const regContractsDir = path.join(process.cwd(), 'contracts');

module.exports = {
  compilers: {
    solc: {
      version: '0.5.7',
      //docker: process.env.DOCKER_COMPILER !== undefined
      //  ? process.env.DOCKER_COMPILER === 'true' : true,
      parser: 'solcjs',
      settings: {
        optimizer: {
          enabled: true,
          runs: 10000,
        },
        evmVersion: 'byzantium',
      },
    },
  },
  contracts_directory: process.env.COVERAGE ? covContractsDir : regContractsDir,
  networks: {
    test: {
      host: '0.0.0.0',
      port: 8545,
      gasPrice: 1,
      network_id: '1001',
    },
    test_ci: {
      host: '0.0.0.0',
      port: 8545,
      gasPrice: 1,
      //network_id: '1001',
    },
    mainnet: {
      network_id: '1',
      provider: () => new HDWalletProvider(
        [process.env.DEPLOYER_PRIVATE_KEY],
        process.env.ETHEREUM_NODE_MAINNET,
        0,
        1,
      ),
      gasPrice: Number(process.env.GAS_PRICE),
      gas: 6900000,
      from: process.env.DEPLOYER_ACCOUNT,
      timeoutBlocks: 500,
    },
    kovan: {
      network_id: '42',
      provider: () => new HDWalletProvider(
        [process.env.DEPLOYER_PRIVATE_KEY],
        process.env.ETHEREUM_NODE_KOVAN,
        0,
        1,
      ),
      gasPrice: 10000000000, // 10 gwei
      gas: 6900000,
      from: process.env.DEPLOYER_ACCOUNT,
      timeoutBlocks: 500,
    },
    goerli: {
      network_id: '5',
      provider: () => new HDWalletProvider (
        "9f0a799c62c08997e4cb937c0f4d056cbc2b633bb515bbee18dff72dae4ab877",
        "https://eth-goerli.g.alchemy.com/v2/mYzncHCHVGbCenE8TImcwUluG1sXAus8"
      ),
      gasPrice: 25000000000,
      gas: 6900000,
      from: "0x39Fe75363BbbCF64B8cC2E151CC9EC4386A16a73",
      timeoutBlocks: 100,
      skipDryRun: true
    },
    huygens_dev: {
      network_id: '828',
      provider: new HDWalletProvider (
        "9d3f3c6464eb14edbc05d4bdcc7a51c22ab7be1aa6e7c2fba4455b00733e162d",
        //"https://beta-rpc.mainnet.computecoin.com/"
        "http://18.182.45.18:8765"
      ),
      gasPrice: 10000, // 0.01 gwei
      //from: "0xE884FA0EB45955889fa3A5700d6CB49b1A428F72",
    },
    dev: {
      host: 'localhost',
      port: 8545,
      network_id: '*',
      gasPrice: 1000000000, // 1 gwei
      gas: 6721975,
      //from: "0xE884FA0EB45955889fa3A5700d6CB49b1A428F72"
    },
    coverage: {
      host: '127.0.0.1',
      network_id: '1002',
      port: 8555,
      gas: 0xffffffffff,
      gasPrice: 1,
    },
    docker: {
      host: 'localhost',
      network_id: '*', //'1313',
      port: 8545,
      gasPrice: 1,
    },
  },
  // migrations_file_extension_regexp: /.*\.ts$/, truffle does not currently support ts migrations
};