<script setup>
    const ssid = defineModel('ssid')
    const pass = defineModel('pass')
    
    const emit = defineEmits([ 'save' ]);
    const props = defineProps({
      title:       { type:String, default:'title'},
      oldsettings: { type:Number, default:'old'},
      
      
    })
</script>
<template>
        <div class="card">
            <p class="card-title">
                <font-awesome-icon icon="fa-wifi"/> {{ title }}
            </p>
            <font-awesome-icon icon="fa-wifi"/>
            <input v-model="ssid" placeholder="SSID" ><br> 
            <font-awesome-icon icon="fa-key"/>
            <input v-model="pass" placeholder="Password" > 
            <p>

              <button class="button-on" @click="$emit('save')">Save WiFi</button>
            </p>
            <p class="state">Old setting: {{ oldsettingsaved }}</p>
          </div>

</template>

<script>


export default {
    
    
    
    data() {
        return {     
           password:    { type:String, default:''},
           oldsettingsaved: this.oldsettings,
        };
    },
    mounted() { this.oldsettingsaved = this.oldsettings },
    methods : { }
}
</script>

