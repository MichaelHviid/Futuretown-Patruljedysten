<script setup>
  const mac = defineModel()
  const emit = defineEmits(['save'])
  const props = defineProps({
    title:       { type: String, default: 'MAC' },
    oldsettings: { type: String, default: '' },
  })
</script>

<template>
  <div class="card">
    <p class="card-title">
      <font-awesome-icon icon="fa-wifi" /> {{ title }}
    </p>
    <font-awesome-icon icon="fa-microchip" />
    <input v-model="mac" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17"
           style="text-transform:uppercase; letter-spacing: 1px; font-family: monospace; width: 170px" />
    <p>
      <button class="button-on" @click="$emit('save')">Save MAC</button>
    </p>
    <p class="state">Gemt: {{ oldsettingsSaved }}</p>
  </div>
</template>

<script>
export default {
  data() {
    return {
      oldsettingsSaved: this.oldsettings,
    }
  },
  mounted() { this.oldsettingsSaved = this.oldsettings },
}
</script>
