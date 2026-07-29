<script setup>
    const settings = defineModel()
    const emit = defineEmits([ 'save' ]);
    const props = defineProps({
      title:       { type:String, default:'title'},
      oldsettings: { type:Number, default:'old'},
      icon:        { type:String, default:'fa-lightbulb'},
      min:         { type:Number, default:0},
      max:         { type:Number, default:255},
      
    })
</script>
<template>
        <div class="card">
            <p class="card-title">
                <font-awesome-icon :icon="icon"/>{{ title }}
            </p>
            <input v-model.number="settings" placeholder="new value" > 
            <p>
              <button class="button-on" @click="$emit('save')">Save</button>
            </p>
            <p class="state">Old setting: {{ oldsettingsaved }}</p>
          </div>

</template>

<script>


export default {
    
    
    
    data() {
        return {     
           oldsettingsaved: this.oldsettings,
        };
    },
    mounted() { this.oldsettingsaved = this.oldsettings },
    methods : { }
}
</script>

