#!/usr/bin/env node
'use strict'
const net = require('net')

const port = Number(process.argv[2]) || 7020
const server = net.createServer((socket) => {
  socket.pipe(socket)
}).listen(port, '0.0.0.0', () => {
  console.log(`server listening on ${port}`)
})
