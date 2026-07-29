import { createApp } from 'vue'
import App from './App.vue'
import packageJson from '../package.json'

/* add icons to the library */
import { library } from '@fortawesome/fontawesome-svg-core'
import { FontAwesomeIcon } from '@fortawesome/vue-fontawesome'
import { fas } from '@fortawesome/free-solid-svg-icons'
import { far } from '@fortawesome/free-regular-svg-icons'

library.add(fas,far)

const app = createApp(App)


app.provide('wdm', {
    'version'       : packageJson.version,
    'versiondate'   : packageJson.versiondate,
    'versiontime'   : packageJson.versiontime,
    'appName'       : packageJson.name,
})

app.component('font-awesome-icon', FontAwesomeIcon)
app.mount('#app')
