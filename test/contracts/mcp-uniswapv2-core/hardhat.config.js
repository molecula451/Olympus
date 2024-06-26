require("@nomiclabs/hardhat-waffle")
require("dotenv").config()
/**
 * @type import('hardhat/config').HardhatUserConfig
 */
module.exports = {
  solidity: {
    version: "0.5.16",
    settings: {
      optimizer: {
        enabled: true,
        runs: 1000,
      },
      evmVersion: "istanbul"
    },
  },
  networks: {
    ccnbeta: {
      chainid: 9700,
      url: process.env.CCNBETA_END_POINT,
      accounts: [process.env.CCNBETA_PRIVATE_KEY, process.env.CCNBETA_OTHER_KEY]
    },
  },
  etherscan: {
    apiKey: process.env.ETHERSCAN_API_KEY
  },
};
