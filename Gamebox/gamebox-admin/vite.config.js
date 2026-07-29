import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    vue(),
    vueDevTools(),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    },
  },
  server: { 
    port: 8080,
    https: false,
    proxy: {
      '/config': {
          target:'http://192.168.1.249/',
          ws:false,
          changeOrigin:true },
      '/ws': {
          target:'ws://192.168.1.249/',
          ws:true,
          changeOrigin:true },
      '/on': {
          target:'http://192.168.1.249/',
          ws:false,
          changeOrigin:true },
      '/off': {
          target:'http://192.168.1.249/',
          ws:false,
          changeOrigin:true },
      }
  },
  
})
